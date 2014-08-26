// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/chromeos_policy.h"

#include <algorithm>
#include <set>
#include <string>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/time/time.h>

#include "update_engine/error_code.h"
#include "update_engine/update_manager/device_policy_provider.h"
#include "update_engine/update_manager/policy_utils.h"
#include "update_engine/update_manager/shill_provider.h"
#include "update_engine/utils.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::ErrorCode;
using std::max;
using std::min;
using std::set;
using std::string;

namespace {

// Increment |url_idx|, |url_num_failures| or none of them based on the provided
// error code. If |url_idx| is incremented, then sets |url_num_failures| to zero
// and returns true; otherwise, returns false.
//
// TODO(garnold) Adapted from PayloadState::UpdateFailed() (to be retired).
bool HandleErrorCode(ErrorCode err_code, int* url_idx, int* url_num_failures) {
  err_code = chromeos_update_engine::utils::GetBaseErrorCode(err_code);
  switch (err_code) {
    // Errors which are good indicators of a problem with a particular URL or
    // the protocol used in the URL or entities in the communication channel
    // (e.g. proxies). We should try the next available URL in the next update
    // check to quickly recover from these errors.
    case ErrorCode::kPayloadHashMismatchError:
    case ErrorCode::kPayloadSizeMismatchError:
    case ErrorCode::kDownloadPayloadVerificationError:
    case ErrorCode::kDownloadPayloadPubKeyVerificationError:
    case ErrorCode::kSignedDeltaPayloadExpectedError:
    case ErrorCode::kDownloadInvalidMetadataMagicString:
    case ErrorCode::kDownloadSignatureMissingInManifest:
    case ErrorCode::kDownloadManifestParseError:
    case ErrorCode::kDownloadMetadataSignatureError:
    case ErrorCode::kDownloadMetadataSignatureVerificationError:
    case ErrorCode::kDownloadMetadataSignatureMismatch:
    case ErrorCode::kDownloadOperationHashVerificationError:
    case ErrorCode::kDownloadOperationExecutionError:
    case ErrorCode::kDownloadOperationHashMismatch:
    case ErrorCode::kDownloadInvalidMetadataSize:
    case ErrorCode::kDownloadInvalidMetadataSignature:
    case ErrorCode::kDownloadOperationHashMissingError:
    case ErrorCode::kDownloadMetadataSignatureMissingError:
    case ErrorCode::kPayloadMismatchedType:
    case ErrorCode::kUnsupportedMajorPayloadVersion:
    case ErrorCode::kUnsupportedMinorPayloadVersion:
      LOG(INFO) << "Advancing download URL due to error "
                << chromeos_update_engine::utils::CodeToString(err_code)
                << " (" << static_cast<int>(err_code) << ")";
      *url_idx += 1;
      *url_num_failures = 0;
      return true;

    // Errors which seem to be just transient network/communication related
    // failures and do not indicate any inherent problem with the URL itself.
    // So, we should keep the current URL but just increment the
    // failure count to give it more chances. This way, while we maximize our
    // chances of downloading from the URLs that appear earlier in the response
    // (because download from a local server URL that appears earlier in a
    // response is preferable than downloading from the next URL which could be
    // an Internet URL and thus could be more expensive).
    case ErrorCode::kError:
    case ErrorCode::kDownloadTransferError:
    case ErrorCode::kDownloadWriteError:
    case ErrorCode::kDownloadStateInitializationError:
    case ErrorCode::kOmahaErrorInHTTPResponse:  // Aggregate for HTTP errors.
      LOG(INFO) << "Incrementing URL failure count due to error "
                << chromeos_update_engine::utils::CodeToString(err_code)
                << " (" << static_cast<int>(err_code) << ")";
      *url_num_failures += 1;
      return false;

    // Errors which are not specific to a URL and hence shouldn't result in
    // the URL being penalized. This can happen in two cases:
    // 1. We haven't started downloading anything: These errors don't cost us
    // anything in terms of actual payload bytes, so we should just do the
    // regular retries at the next update check.
    // 2. We have successfully downloaded the payload: In this case, the
    // payload attempt number would have been incremented and would take care
    // of the back-off at the next update check.
    // In either case, there's no need to update URL index or failure count.
    case ErrorCode::kOmahaRequestError:
    case ErrorCode::kOmahaResponseHandlerError:
    case ErrorCode::kPostinstallRunnerError:
    case ErrorCode::kFilesystemCopierError:
    case ErrorCode::kInstallDeviceOpenError:
    case ErrorCode::kKernelDeviceOpenError:
    case ErrorCode::kDownloadNewPartitionInfoError:
    case ErrorCode::kNewRootfsVerificationError:
    case ErrorCode::kNewKernelVerificationError:
    case ErrorCode::kPostinstallBootedFromFirmwareB:
    case ErrorCode::kPostinstallFirmwareRONotUpdatable:
    case ErrorCode::kOmahaRequestEmptyResponseError:
    case ErrorCode::kOmahaRequestXMLParseError:
    case ErrorCode::kOmahaResponseInvalid:
    case ErrorCode::kOmahaUpdateIgnoredPerPolicy:
    case ErrorCode::kOmahaUpdateDeferredPerPolicy:
    case ErrorCode::kOmahaUpdateDeferredForBackoff:
    case ErrorCode::kPostinstallPowerwashError:
    case ErrorCode::kUpdateCanceledByChannelChange:
    case ErrorCode::kOmahaRequestXMLHasEntityDecl:
      LOG(INFO) << "Not changing URL index or failure count due to error "
                << chromeos_update_engine::utils::CodeToString(err_code)
                << " (" << static_cast<int>(err_code) << ")";
      return false;

    case ErrorCode::kSuccess:                            // success code
    case ErrorCode::kUmaReportedMax:                     // not an error code
    case ErrorCode::kOmahaRequestHTTPResponseBase:       // aggregated already
    case ErrorCode::kDevModeFlag:                       // not an error code
    case ErrorCode::kResumedFlag:                        // not an error code
    case ErrorCode::kTestImageFlag:                      // not an error code
    case ErrorCode::kTestOmahaUrlFlag:                   // not an error code
    case ErrorCode::kSpecialFlags:                       // not an error code
      // These shouldn't happen. Enumerating these  explicitly here so that we
      // can let the compiler warn about new error codes that are added to
      // action_processor.h but not added here.
      LOG(WARNING) << "Unexpected error "
                   << chromeos_update_engine::utils::CodeToString(err_code)
                   << " (" << static_cast<int>(err_code) << ")";
    // Note: Not adding a default here so as to let the compiler warn us of
    // any new enums that were added in the .h but not listed in this switch.
  }
  return false;
}

// Checks whether |download_url| can be used under given download restrictions.
bool DownloadUrlIsUsable(const string& download_url, bool http_allowed) {
  return http_allowed || !StartsWithASCII(download_url, "http://", false);
}

}  // namespace

namespace chromeos_update_manager {

EvalStatus ChromeOSPolicy::UpdateCheckAllowed(
    EvaluationContext* ec, State* state, string* error,
    UpdateCheckParams* result) const {
  // Set the default return values.
  result->updates_enabled = true;
  result->target_channel.clear();
  result->target_version_prefix.clear();
  result->is_interactive = false;

  DevicePolicyProvider* const dp_provider = state->device_policy_provider();
  UpdaterProvider* const updater_provider = state->updater_provider();
  SystemProvider* const system_provider = state->system_provider();

  // Do not perform any updates if booted from removable device. This decision
  // is final.
  const bool* is_boot_device_removable_p = ec->GetValue(
      system_provider->var_is_boot_device_removable());
  if (is_boot_device_removable_p && *is_boot_device_removable_p) {
    result->updates_enabled = false;
    return EvalStatus::kSucceeded;
  }

  const bool* device_policy_is_loaded_p = ec->GetValue(
      dp_provider->var_device_policy_is_loaded());
  if (device_policy_is_loaded_p && *device_policy_is_loaded_p) {
    // Check whether updates are disabled by policy.
    const bool* update_disabled_p = ec->GetValue(
        dp_provider->var_update_disabled());
    if (update_disabled_p && *update_disabled_p)
      return EvalStatus::kAskMeAgainLater;

    // Determine whether a target version prefix is dictated by policy.
    const string* target_version_prefix_p = ec->GetValue(
        dp_provider->var_target_version_prefix());
    if (target_version_prefix_p)
      result->target_version_prefix = *target_version_prefix_p;

    // Determine whether a target channel is dictated by policy.
    const bool* release_channel_delegated_p = ec->GetValue(
        dp_provider->var_release_channel_delegated());
    if (release_channel_delegated_p && !(*release_channel_delegated_p)) {
      const string* release_channel_p = ec->GetValue(
          dp_provider->var_release_channel());
      if (release_channel_p)
        result->target_channel = *release_channel_p;
    }
  }

  // First, check to see if an interactive update was requested.
  const bool* interactive_update_requested_p = ec->GetValue(
      updater_provider->var_interactive_update_requested());
  if (interactive_update_requested_p && *interactive_update_requested_p) {
    result->is_interactive = true;
    return EvalStatus::kSucceeded;
  }

  // The logic thereafter applies to periodic updates. Bear in mind that we
  // should not return a final "no" if any of these criteria are not satisfied,
  // because the system may still update due to an interactive update request.

  // Unofficial builds should not perform periodic update checks.
  const bool* is_official_build_p = ec->GetValue(
      system_provider->var_is_official_build());
  if (is_official_build_p && !(*is_official_build_p)) {
    return EvalStatus::kAskMeAgainLater;
  }

  // If OOBE is enabled, wait until it is completed.
  const bool* is_oobe_enabled_p = ec->GetValue(
      state->config_provider()->var_is_oobe_enabled());
  if (is_oobe_enabled_p && *is_oobe_enabled_p) {
    const bool* is_oobe_complete_p = ec->GetValue(
        system_provider->var_is_oobe_complete());
    if (is_oobe_complete_p && !(*is_oobe_complete_p))
      return EvalStatus::kAskMeAgainLater;
  }

  // Ensure that periodic update checks are timed properly.
  Time next_update_check;
  if (NextUpdateCheckTime(ec, state, error, &next_update_check) !=
      EvalStatus::kSucceeded) {
    return EvalStatus::kFailed;
  }
  if (!ec->IsWallclockTimeGreaterThan(next_update_check))
    return EvalStatus::kAskMeAgainLater;

  // It is time to check for an update.
  return EvalStatus::kSucceeded;
}

EvalStatus ChromeOSPolicy::UpdateCanStart(
    EvaluationContext* ec,
    State* state,
    string* error,
    UpdateDownloadParams* result,
    const bool interactive,
    const UpdateState& update_state) const {
  // Set the default return values.
  result->update_can_start = true;
  result->p2p_allowed = false;
  result->cannot_start_reason = UpdateCannotStartReason::kUndefined;
  result->scatter_wait_period = kZeroInterval;
  result->scatter_check_threshold = 0;
  result->download_url_idx = -1;
  result->download_url_num_failures = 0;

  // Make sure that we're not due for an update check.
  UpdateCheckParams check_result;
  EvalStatus check_status = UpdateCheckAllowed(ec, state, error, &check_result);
  if (check_status == EvalStatus::kFailed)
    return EvalStatus::kFailed;
  if (check_status == EvalStatus::kSucceeded &&
      check_result.updates_enabled == true) {
    result->update_can_start = false;
    result->cannot_start_reason = UpdateCannotStartReason::kCheckDue;
    return EvalStatus::kSucceeded;
  }

  DevicePolicyProvider* const dp_provider = state->device_policy_provider();

  const bool* device_policy_is_loaded_p = ec->GetValue(
      dp_provider->var_device_policy_is_loaded());
  if (device_policy_is_loaded_p && *device_policy_is_loaded_p) {
    // Check whether scattering applies to this update attempt. We should not be
    // scattering if this is an interactive update check, or if OOBE is enabled
    // but not completed.
    //
    // Note: current code further suppresses scattering if a "deadline"
    // attribute is found in the Omaha response. However, it appears that the
    // presence of this attribute is merely indicative of an OOBE update, during
    // which we suppress scattering anyway.
    bool scattering_applies = false;
    if (!interactive) {
      const bool* is_oobe_enabled_p = ec->GetValue(
          state->config_provider()->var_is_oobe_enabled());
      if (is_oobe_enabled_p && !(*is_oobe_enabled_p)) {
        scattering_applies = true;
      } else {
        const bool* is_oobe_complete_p = ec->GetValue(
            state->system_provider()->var_is_oobe_complete());
        scattering_applies = (is_oobe_complete_p && *is_oobe_complete_p);
      }
    }

    // Compute scattering values.
    if (scattering_applies) {
      UpdateScatteringResult scatter_result;
      EvalStatus scattering_status = UpdateScattering(
          ec, state, error, &scatter_result, update_state);
      if (scattering_status != EvalStatus::kSucceeded ||
          scatter_result.is_scattering) {
        if (scattering_status != EvalStatus::kFailed) {
          result->update_can_start = false;
          result->cannot_start_reason = UpdateCannotStartReason::kScattering;
          result->scatter_wait_period = scatter_result.wait_period;
          result->scatter_check_threshold = scatter_result.check_threshold;
        }
        return scattering_status;
      }
    }

    // Determine whether use of P2P is allowed by policy.
    const bool* policy_au_p2p_enabled_p = ec->GetValue(
        dp_provider->var_au_p2p_enabled());
    result->p2p_allowed = policy_au_p2p_enabled_p && *policy_au_p2p_enabled_p;
  }

  // Enable P2P, if so mandated by the updater configuration.
  if (!result->p2p_allowed) {
    const bool* updater_p2p_enabled_p = ec->GetValue(
        state->updater_provider()->var_p2p_enabled());
    result->p2p_allowed = updater_p2p_enabled_p && *updater_p2p_enabled_p;
  }

  // Determine the URL to download the update from. Note that a failure to find
  // a download URL will only fail this policy iff no other means of download
  // (such as P2P) are enabled.
  UpdateDownloadUrlResult download_url_result;
  EvalStatus download_url_status = UpdateDownloadUrl(
      ec, state, error, &download_url_result, update_state);
  if (download_url_status == EvalStatus::kSucceeded) {
    result->download_url_idx = download_url_result.url_idx;
    result->download_url_num_failures = download_url_result.url_num_failures;
  } else if (!result->p2p_allowed) {
    if (download_url_status != EvalStatus::kFailed) {
      result->update_can_start = false;
      result->cannot_start_reason = UpdateCannotStartReason::kCannotDownload;
    }
    return download_url_status;
  }

  return EvalStatus::kSucceeded;
}

// TODO(garnold) Logic in this method is based on
// ConnectionManager::IsUpdateAllowedOver(); be sure to deprecate the latter.
//
// TODO(garnold) The current logic generally treats the list of allowed
// connections coming from the device policy as a whitelist, meaning that it
// can only be used for enabling connections, but not disable them. Further,
// certain connection types (like Bluetooth) cannot be enabled even by policy.
// In effect, the only thing that device policy can change is to enable
// updates over a cellular network (disabled by default). We may want to
// revisit this semantics, allowing greater flexibility in defining specific
// permissions over all types of networks.
EvalStatus ChromeOSPolicy::UpdateDownloadAllowed(
    EvaluationContext* ec,
    State* state,
    string* error,
    bool* result) const {
  // Get the current connection type.
  ShillProvider* const shill_provider = state->shill_provider();
  const ConnectionType* conn_type_p = ec->GetValue(
      shill_provider->var_conn_type());
  POLICY_CHECK_VALUE_AND_FAIL(conn_type_p, error);
  ConnectionType conn_type = *conn_type_p;

  // If we're tethering, treat it as a cellular connection.
  if (conn_type != ConnectionType::kCellular) {
    const ConnectionTethering* conn_tethering_p = ec->GetValue(
        shill_provider->var_conn_tethering());
    POLICY_CHECK_VALUE_AND_FAIL(conn_tethering_p, error);
    if (*conn_tethering_p == ConnectionTethering::kConfirmed)
      conn_type = ConnectionType::kCellular;
  }

  // By default, we allow updates for all connection types, with exceptions as
  // noted below. This also determines whether a device policy can override the
  // default.
  *result = true;
  bool device_policy_can_override = false;
  switch (conn_type) {
    case ConnectionType::kBluetooth:
      *result = false;
      break;

    case ConnectionType::kCellular:
      *result = false;
      device_policy_can_override = true;
      break;

    case ConnectionType::kUnknown:
      if (error)
        *error = "Unknown connection type";
      return EvalStatus::kFailed;

    default:
      break;  // Nothing to do.
  }

  // If update is allowed, we're done.
  if (*result)
    return EvalStatus::kSucceeded;

  // Check whether the device policy specifically allows this connection.
  if (device_policy_can_override) {
    DevicePolicyProvider* const dp_provider = state->device_policy_provider();
    const bool* device_policy_is_loaded_p = ec->GetValue(
        dp_provider->var_device_policy_is_loaded());
    if (device_policy_is_loaded_p && *device_policy_is_loaded_p) {
      const set<ConnectionType>* allowed_conn_types_p = ec->GetValue(
          dp_provider->var_allowed_connection_types_for_update());
      if (allowed_conn_types_p) {
        if (allowed_conn_types_p->count(conn_type)) {
          *result = true;
          return EvalStatus::kSucceeded;
        }
      } else if (conn_type == ConnectionType::kCellular) {
        // Local user settings can allow updates over cellular iff a policy was
        // loaded but no allowed connections were specified in it.
        const bool* update_over_cellular_allowed_p = ec->GetValue(
            state->updater_provider()->var_cellular_enabled());
        if (update_over_cellular_allowed_p && *update_over_cellular_allowed_p)
          *result = true;
      }
    }
  }

  return (*result ? EvalStatus::kSucceeded : EvalStatus::kAskMeAgainLater);
}

EvalStatus ChromeOSPolicy::NextUpdateCheckTime(EvaluationContext* ec,
                                               State* state, string* error,
                                               Time* next_update_check) const {
  UpdaterProvider* const updater_provider = state->updater_provider();

  // Don't check for updates too often. We limit the update checks to once every
  // some interval. The interval is kTimeoutInitialInterval the first time and
  // kTimeoutPeriodicInterval for the subsequent update checks. If the update
  // check fails, we increase the interval between the update checks
  // exponentially until kTimeoutMaxBackoffInterval. Finally, to avoid having
  // many chromebooks running update checks at the exact same time, we add some
  // fuzz to the interval.
  const Time* updater_started_time =
      ec->GetValue(updater_provider->var_updater_started_time());
  POLICY_CHECK_VALUE_AND_FAIL(updater_started_time, error);

  const base::Time* last_checked_time =
      ec->GetValue(updater_provider->var_last_checked_time());

  const uint64_t* seed = ec->GetValue(state->random_provider()->var_seed());
  POLICY_CHECK_VALUE_AND_FAIL(seed, error);

  PRNG prng(*seed);

  // If this is the first attempt, compute and return an initial value.
  if (!last_checked_time || *last_checked_time < *updater_started_time) {
    *next_update_check = *updater_started_time + FuzzedInterval(
        &prng, kTimeoutInitialInterval, kTimeoutRegularFuzz);
    return EvalStatus::kSucceeded;
  }

  // Check whether the server is enforcing a poll interval; if not, this value
  // will be zero.
  const unsigned int* server_dictated_poll_interval = ec->GetValue(
      updater_provider->var_server_dictated_poll_interval());
  POLICY_CHECK_VALUE_AND_FAIL(server_dictated_poll_interval, error);

  int interval = *server_dictated_poll_interval;
  int fuzz = 0;

  // If no poll interval was dictated by server compute a back-off period,
  // starting from a predetermined base periodic interval and increasing
  // exponentially by the number of consecutive failed attempts.
  if (interval == 0) {
    const unsigned int* consecutive_failed_update_checks = ec->GetValue(
        updater_provider->var_consecutive_failed_update_checks());
    POLICY_CHECK_VALUE_AND_FAIL(consecutive_failed_update_checks, error);

    interval = kTimeoutPeriodicInterval;
    unsigned int num_failures = *consecutive_failed_update_checks;
    while (interval < kTimeoutMaxBackoffInterval && num_failures) {
      interval *= 2;
      num_failures--;
    }
  }

  // We cannot back off longer than the predetermined maximum interval.
  if (interval > kTimeoutMaxBackoffInterval)
    interval = kTimeoutMaxBackoffInterval;

  // We cannot back off shorter than the predetermined periodic interval. Also,
  // in this case set the fuzz to a predetermined regular value.
  if (interval <= kTimeoutPeriodicInterval) {
    interval = kTimeoutPeriodicInterval;
    fuzz = kTimeoutRegularFuzz;
  }

  // If not otherwise determined, defer to a fuzz of +/-(interval / 2).
  if (fuzz == 0)
    fuzz = interval;

  *next_update_check = *last_checked_time + FuzzedInterval(
      &prng, interval, fuzz);
  return EvalStatus::kSucceeded;
}

TimeDelta ChromeOSPolicy::FuzzedInterval(PRNG* prng, int interval, int fuzz) {
  DCHECK_GE(interval, 0);
  DCHECK_GE(fuzz, 0);
  int half_fuzz = fuzz / 2;
  // This guarantees the output interval is non negative.
  int interval_min = max(interval - half_fuzz, 0);
  int interval_max = interval + half_fuzz;
  return TimeDelta::FromSeconds(prng->RandMinMax(interval_min, interval_max));
}

EvalStatus ChromeOSPolicy::UpdateDownloadUrl(
    EvaluationContext* ec, State* state, std::string* error,
    UpdateDownloadUrlResult* result, const UpdateState& update_state) const {
  int url_idx = 0;
  int url_num_failures = 0;
  if (update_state.num_checks > 1) {
    // Ignore negative URL indexes, which indicate that no previous suitable
    // download URL was found.
    url_idx = max(0, update_state.download_url_idx);
    url_num_failures = update_state.download_url_num_failures;
  }

  // Preconditions / sanity checks.
  DCHECK_GE(update_state.download_failures_max, 0);
  DCHECK_LT(url_idx, static_cast<int>(update_state.download_urls.size()));
  DCHECK_LE(url_num_failures, update_state.download_failures_max);

  // Determine whether HTTP downloads are forbidden by policy. This only
  // applies to official system builds; otherwise, HTTP is always enabled.
  bool http_allowed = true;
  const bool* is_official_build_p = ec->GetValue(
      state->system_provider()->var_is_official_build());
  if (is_official_build_p && *is_official_build_p) {
    DevicePolicyProvider* const dp_provider = state->device_policy_provider();
    const bool* device_policy_is_loaded_p = ec->GetValue(
        dp_provider->var_device_policy_is_loaded());
    if (device_policy_is_loaded_p && *device_policy_is_loaded_p) {
      const bool* policy_http_downloads_enabled_p = ec->GetValue(
          dp_provider->var_http_downloads_enabled());
      http_allowed = (!policy_http_downloads_enabled_p ||
                      *policy_http_downloads_enabled_p);
    }
  }

  // Process recent failures, stop if the URL index advances.
  for (auto& err_code : update_state.download_url_error_codes) {
    if (HandleErrorCode(err_code, &url_idx, &url_num_failures))
      break;
    if (url_num_failures > update_state.download_failures_max) {
      url_idx++;
      url_num_failures = 0;
      break;
    }
  }
  url_idx %= update_state.download_urls.size();

  // Scan through URLs until a usable one is found.
  const int start_url_idx = url_idx;
  while (!DownloadUrlIsUsable(update_state.download_urls[url_idx],
                              http_allowed)) {
    url_idx = (url_idx + 1) % update_state.download_urls.size();
    url_num_failures = 0;
    if (url_idx == start_url_idx)
      return EvalStatus::kFailed;  // No usable URLs.
  }

  result->url_idx = url_idx;
  result->url_num_failures = url_num_failures;
  return EvalStatus::kSucceeded;
}

EvalStatus ChromeOSPolicy::UpdateScattering(
    EvaluationContext* ec,
    State* state,
    string* error,
    UpdateScatteringResult* result,
    const UpdateState& update_state) const {
  // Preconditions. These stem from the postconditions and usage contract.
  DCHECK(update_state.scatter_wait_period >= kZeroInterval);
  DCHECK_GE(update_state.scatter_check_threshold, 0);

  // Set default result values.
  result->is_scattering = false;
  result->wait_period = kZeroInterval;
  result->check_threshold = 0;

  DevicePolicyProvider* const dp_provider = state->device_policy_provider();

  // Ensure that a device policy is loaded.
  const bool* device_policy_is_loaded_p = ec->GetValue(
      dp_provider->var_device_policy_is_loaded());
  if (!(device_policy_is_loaded_p && *device_policy_is_loaded_p))
    return EvalStatus::kSucceeded;

  // Is scattering enabled by policy?
  const TimeDelta* scatter_factor_p = ec->GetValue(
      dp_provider->var_scatter_factor());
  if (!scatter_factor_p || *scatter_factor_p == kZeroInterval)
    return EvalStatus::kSucceeded;

  // Obtain a pseudo-random number generator.
  const uint64_t* seed = ec->GetValue(state->random_provider()->var_seed());
  POLICY_CHECK_VALUE_AND_FAIL(seed, error);
  PRNG prng(*seed);

  // Step 1: Maintain the scattering wait period.
  //
  // If no wait period was previously determined, or it no longer fits in the
  // scatter factor, then generate a new one. Otherwise, keep the one we have.
  TimeDelta wait_period = update_state.scatter_wait_period;
  if (wait_period == kZeroInterval || wait_period > *scatter_factor_p) {
    wait_period = TimeDelta::FromSeconds(
        prng.RandMinMax(1, scatter_factor_p->InSeconds()));
  }

  // If we surpass the wait period or the max scatter period associated with
  // the update, then no wait is needed.
  Time wait_expires = (update_state.first_seen +
                       min(wait_period, update_state.scatter_wait_period_max));
  if (ec->IsWallclockTimeGreaterThan(wait_expires))
    wait_period = kZeroInterval;

  // Step 2: Maintain the update check threshold count.
  //
  // If an update check threshold is not specified then generate a new
  // one.
  int check_threshold = update_state.scatter_check_threshold;
  if (check_threshold == 0) {
    check_threshold = prng.RandMinMax(
        update_state.scatter_check_threshold_min,
        update_state.scatter_check_threshold_max);
  }

  // If the update check threshold is not within allowed range then nullify it.
  // TODO(garnold) This is compliant with current logic found in
  // OmahaRequestAction::IsUpdateCheckCountBasedWaitingSatisfied(). We may want
  // to change it so that it behaves similarly to the wait period case, namely
  // if the current value exceeds the maximum, we set a new one within range.
  if (check_threshold > update_state.scatter_check_threshold_max)
    check_threshold = 0;

  // If the update check threshold is non-zero and satisfied, then nullify it.
  if (check_threshold > 0 && update_state.num_checks >= check_threshold)
    check_threshold = 0;

  bool is_scattering = (wait_period != kZeroInterval || check_threshold);
  EvalStatus ret = EvalStatus::kSucceeded;
  if (is_scattering && wait_period == update_state.scatter_wait_period &&
      check_threshold == update_state.scatter_check_threshold)
    ret = EvalStatus::kAskMeAgainLater;
  result->is_scattering = is_scattering;
  result->wait_period = wait_period;
  result->check_threshold = check_threshold;
  return ret;
}

}  // namespace chromeos_update_manager
