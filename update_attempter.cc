// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_attempter.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/rand_util.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include <glib.h>
#include <metrics/metrics_library.h>
#include <policy/device_policy.h>
#include <policy/libpolicy.h>

#include "update_engine/certificate_checker.h"
#include "update_engine/clock_interface.h"
#include "update_engine/constants.h"
#include "update_engine/dbus_service.h"
#include "update_engine/dbus_wrapper_interface.h"
#include "update_engine/download_action.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/glib_utils.h"
#include "update_engine/hardware_interface.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/metrics.h"
#include "update_engine/multi_range_http_fetcher.h"
#include "update_engine/omaha_request_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/p2p_manager.h"
#include "update_engine/payload_state_interface.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/subprocess.h"
#include "update_engine/system_state.h"
#include "update_engine/update_manager/policy.h"
#include "update_engine/update_manager/update_manager.h"
#include "update_engine/utils.h"

using base::Bind;
using base::Callback;
using base::StringPrintf;
using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using chromeos_update_manager::EvalStatus;
using chromeos_update_manager::Policy;
using chromeos_update_manager::UpdateCheckParams;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;

namespace chromeos_update_engine {

const int UpdateAttempter::kMaxDeltaUpdateFailures = 3;

namespace {
const int kMaxConsecutiveObeyProxyRequests = 20;

const char* kUpdateCompletedMarker =
    "/var/run/update_engine_autoupdate_completed";
}  // namespace

const char* UpdateStatusToString(UpdateStatus status) {
  switch (status) {
    case UPDATE_STATUS_IDLE:
      return update_engine::kUpdateStatusIdle;
    case UPDATE_STATUS_CHECKING_FOR_UPDATE:
      return update_engine::kUpdateStatusCheckingForUpdate;
    case UPDATE_STATUS_UPDATE_AVAILABLE:
      return update_engine::kUpdateStatusUpdateAvailable;
    case UPDATE_STATUS_DOWNLOADING:
      return update_engine::kUpdateStatusDownloading;
    case UPDATE_STATUS_VERIFYING:
      return update_engine::kUpdateStatusVerifying;
    case UPDATE_STATUS_FINALIZING:
      return update_engine::kUpdateStatusFinalizing;
    case UPDATE_STATUS_UPDATED_NEED_REBOOT:
      return update_engine::kUpdateStatusUpdatedNeedReboot;
    case UPDATE_STATUS_REPORTING_ERROR_EVENT:
      return update_engine::kUpdateStatusReportingErrorEvent;
    case UPDATE_STATUS_ATTEMPTING_ROLLBACK:
      return update_engine::kUpdateStatusAttemptingRollback;
    case UPDATE_STATUS_DISABLED:
      return update_engine::kUpdateStatusDisabled;
    default:
      return "unknown status";
  }
}

// Turns a generic ErrorCode::kError to a generic error code specific
// to |action| (e.g., ErrorCode::kFilesystemCopierError). If |code| is
// not ErrorCode::kError, or the action is not matched, returns |code|
// unchanged.
ErrorCode GetErrorCodeForAction(AbstractAction* action,
                                     ErrorCode code) {
  if (code != ErrorCode::kError)
    return code;

  const string type = action->Type();
  if (type == OmahaRequestAction::StaticType())
    return ErrorCode::kOmahaRequestError;
  if (type == OmahaResponseHandlerAction::StaticType())
    return ErrorCode::kOmahaResponseHandlerError;
  if (type == FilesystemCopierAction::StaticType())
    return ErrorCode::kFilesystemCopierError;
  if (type == PostinstallRunnerAction::StaticType())
    return ErrorCode::kPostinstallRunnerError;

  return code;
}

UpdateAttempter::UpdateAttempter(SystemState* system_state,
                                 DBusWrapperInterface* dbus_iface)
    : UpdateAttempter(system_state, dbus_iface, kUpdateCompletedMarker) {}

UpdateAttempter::UpdateAttempter(SystemState* system_state,
                                 DBusWrapperInterface* dbus_iface,
                                 const string& update_completed_marker)
    : processor_(new ActionProcessor()),
      system_state_(system_state),
      dbus_iface_(dbus_iface),
      chrome_proxy_resolver_(dbus_iface),
      update_completed_marker_(update_completed_marker) {
  if (!update_completed_marker_.empty() &&
      utils::FileExists(update_completed_marker_.c_str())) {
    status_ = UPDATE_STATUS_UPDATED_NEED_REBOOT;
  } else {
    status_ = UPDATE_STATUS_IDLE;
  }
}

UpdateAttempter::~UpdateAttempter() {
  CleanupCpuSharesManagement();
}

void UpdateAttempter::Init() {
  // Pulling from the SystemState can only be done after construction, since
  // this is an aggregate of various objects (such as the UpdateAttempter),
  // which requires them all to be constructed prior to it being used.
  prefs_ = system_state_->prefs();
  omaha_request_params_ = system_state_->request_params();
}

void UpdateAttempter::ScheduleUpdates() {
  if (IsUpdateRunningOrScheduled())
    return;

  chromeos_update_manager::UpdateManager* const update_manager =
      system_state_->update_manager();
  CHECK(update_manager);
  Callback<void(EvalStatus, const UpdateCheckParams&)> callback = Bind(
      &UpdateAttempter::OnUpdateScheduled, base::Unretained(this));
  // We limit the async policy request to a reasonably short time, to avoid a
  // starvation due to a transient bug.
  update_manager->AsyncPolicyRequest(callback, &Policy::UpdateCheckAllowed);
  waiting_for_scheduled_check_ = true;
}

bool UpdateAttempter::CheckAndReportDailyMetrics() {
  int64_t stored_value;
  Time now = system_state_->clock()->GetWallclockTime();
  if (system_state_->prefs()->Exists(kPrefsDailyMetricsLastReportedAt) &&
      system_state_->prefs()->GetInt64(kPrefsDailyMetricsLastReportedAt,
                                       &stored_value)) {
    Time last_reported_at = Time::FromInternalValue(stored_value);
    TimeDelta time_reported_since = now - last_reported_at;
    if (time_reported_since.InSeconds() < 0) {
      LOG(WARNING) << "Last reported daily metrics "
                   << utils::FormatTimeDelta(time_reported_since) << " ago "
                   << "which is negative. Either the system clock is wrong or "
                   << "the kPrefsDailyMetricsLastReportedAt state variable "
                   << "is wrong.";
      // In this case, report daily metrics to reset.
    } else {
      if (time_reported_since.InSeconds() < 24*60*60) {
        LOG(INFO) << "Last reported daily metrics "
                  << utils::FormatTimeDelta(time_reported_since) << " ago.";
        return false;
      }
      LOG(INFO) << "Last reported daily metrics "
                << utils::FormatTimeDelta(time_reported_since) << " ago, "
                << "which is more than 24 hours ago.";
    }
  }

  LOG(INFO) << "Reporting daily metrics.";
  system_state_->prefs()->SetInt64(kPrefsDailyMetricsLastReportedAt,
                                   now.ToInternalValue());

  ReportOSAge();

  return true;
}

void UpdateAttempter::ReportOSAge() {
  struct stat sb;

  if (system_state_ == nullptr)
    return;

  if (stat("/etc/lsb-release", &sb) != 0) {
    PLOG(ERROR) << "Error getting file status for /etc/lsb-release "
                << "(Note: this may happen in some unit tests)";
    return;
  }

  Time lsb_release_timestamp = utils::TimeFromStructTimespec(&sb.st_ctim);
  Time now = system_state_->clock()->GetWallclockTime();
  TimeDelta age = now - lsb_release_timestamp;
  if (age.InSeconds() < 0) {
    LOG(ERROR) << "The OS age (" << utils::FormatTimeDelta(age)
               << ") is negative. Maybe the clock is wrong? "
               << "(Note: this may happen in some unit tests.)";
    return;
  }

  string metric = "Installer.OSAgeDays";
  LOG(INFO) << "Uploading " << utils::FormatTimeDelta(age)
            << " for metric " <<  metric;
  system_state_->metrics_lib()->SendToUMA(
       metric,
       static_cast<int>(age.InDays()),
       0,             // min: 0 days
       6*30,          // max: 6 months (approx)
       kNumDefaultUmaBuckets);

  metrics::ReportDailyMetrics(system_state_, age);
}

void UpdateAttempter::Update(const string& app_version,
                             const string& omaha_url,
                             const string& target_channel,
                             const string& target_version_prefix,
                             bool obey_proxies,
                             bool interactive) {
  // This is normally called frequently enough so it's appropriate to use as a
  // hook for reporting daily metrics.
  // TODO(garnold) This should be hooked to a separate (reliable and consistent)
  // timeout event.
  CheckAndReportDailyMetrics();

  // Notify of the new update attempt, clearing prior interactive requests.
  if (forced_update_pending_callback_.get())
    forced_update_pending_callback_->Run(false, false);

  chrome_proxy_resolver_.Init();
  fake_update_success_ = false;
  if (status_ == UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    // Although we have applied an update, we still want to ping Omaha
    // to ensure the number of active statistics is accurate.
    //
    // Also convey to the UpdateEngine.Check.Result metric that we're
    // not performing an update check because of this.
    LOG(INFO) << "Not updating b/c we already updated and we're waiting for "
              << "reboot, we'll ping Omaha instead";
    metrics::ReportUpdateCheckMetrics(system_state_,
                                      metrics::CheckResult::kRebootPending,
                                      metrics::CheckReaction::kUnset,
                                      metrics::DownloadErrorCode::kUnset);
    PingOmaha();
    return;
  }
  if (status_ != UPDATE_STATUS_IDLE) {
    // Update in progress. Do nothing
    return;
  }

  if (!CalculateUpdateParams(app_version,
                             omaha_url,
                             target_channel,
                             target_version_prefix,
                             obey_proxies,
                             interactive)) {
    return;
  }

  BuildUpdateActions(interactive);

  SetStatusAndNotify(UPDATE_STATUS_CHECKING_FOR_UPDATE);

  // Update the last check time here; it may be re-updated when an Omaha
  // response is received, but this will prevent us from repeatedly scheduling
  // checks in the case where a response is not received.
  UpdateLastCheckedTime();

  // Just in case we didn't update boot flags yet, make sure they're updated
  // before any update processing starts.
  start_action_processor_ = true;
  UpdateBootFlags();
}

void UpdateAttempter::RefreshDevicePolicy() {
  // Lazy initialize the policy provider, or reload the latest policy data.
  if (!policy_provider_.get())
    policy_provider_.reset(new policy::PolicyProvider());
  policy_provider_->Reload();

  const policy::DevicePolicy* device_policy = nullptr;
  if (policy_provider_->device_policy_is_loaded())
    device_policy = &policy_provider_->GetDevicePolicy();

  if (device_policy)
    LOG(INFO) << "Device policies/settings present";
  else
    LOG(INFO) << "No device policies/settings present.";

  system_state_->set_device_policy(device_policy);
  system_state_->p2p_manager()->SetDevicePolicy(device_policy);
}

void UpdateAttempter::CalculateP2PParams(bool interactive) {
  bool use_p2p_for_downloading = false;
  bool use_p2p_for_sharing = false;

  // Never use p2p for downloading in interactive checks unless the
  // developer has opted in for it via a marker file.
  //
  // (Why would a developer want to opt in? If he's working on the
  // update_engine or p2p codebases so he can actually test his
  // code.).

  if (system_state_ != nullptr) {
    if (!system_state_->p2p_manager()->IsP2PEnabled()) {
      LOG(INFO) << "p2p is not enabled - disallowing p2p for both"
                << " downloading and sharing.";
    } else {
      // Allow p2p for sharing, even in interactive checks.
      use_p2p_for_sharing = true;
      if (!interactive) {
        LOG(INFO) << "Non-interactive check - allowing p2p for downloading";
        use_p2p_for_downloading = true;
      } else {
        LOG(INFO) << "Forcibly disabling use of p2p for downloading "
                  << "since this update attempt is interactive.";
      }
    }
  }

  omaha_request_params_->set_use_p2p_for_downloading(use_p2p_for_downloading);
  omaha_request_params_->set_use_p2p_for_sharing(use_p2p_for_sharing);
}

bool UpdateAttempter::CalculateUpdateParams(const string& app_version,
                                            const string& omaha_url,
                                            const string& target_channel,
                                            const string& target_version_prefix,
                                            bool obey_proxies,
                                            bool interactive) {
  http_response_code_ = 0;

  // Refresh the policy before computing all the update parameters.
  RefreshDevicePolicy();

  // Set the target version prefix, if provided.
  if (!target_version_prefix.empty())
    omaha_request_params_->set_target_version_prefix(target_version_prefix);

  CalculateScatteringParams(interactive);

  CalculateP2PParams(interactive);
  if (omaha_request_params_->use_p2p_for_downloading() ||
      omaha_request_params_->use_p2p_for_sharing()) {
    // OK, p2p is to be used - start it and perform housekeeping.
    if (!StartP2PAndPerformHousekeeping()) {
      // If this fails, disable p2p for this attempt
      LOG(INFO) << "Forcibly disabling use of p2p since starting p2p or "
                << "performing housekeeping failed.";
      omaha_request_params_->set_use_p2p_for_downloading(false);
      omaha_request_params_->set_use_p2p_for_sharing(false);
    }
  }

  if (!omaha_request_params_->Init(app_version,
                                   omaha_url,
                                   interactive)) {
    LOG(ERROR) << "Unable to initialize Omaha request params.";
    return false;
  }

  // Set the target channel, if one was provided.
  if (target_channel.empty()) {
    LOG(INFO) << "No target channel mandated by policy.";
  } else {
    LOG(INFO) << "Setting target channel as mandated: " << target_channel;
    // Pass in false for powerwash_allowed until we add it to the policy
    // protobuf.
    omaha_request_params_->SetTargetChannel(target_channel, false);

    // Since this is the beginning of a new attempt, update the download
    // channel. The download channel won't be updated until the next attempt,
    // even if target channel changes meanwhile, so that how we'll know if we
    // should cancel the current download attempt if there's such a change in
    // target channel.
    omaha_request_params_->UpdateDownloadChannel();
  }

  LOG(INFO) << "target_version_prefix = "
            << omaha_request_params_->target_version_prefix()
            << ", scatter_factor_in_seconds = "
            << utils::FormatSecs(scatter_factor_.InSeconds());

  LOG(INFO) << "Wall Clock Based Wait Enabled = "
            << omaha_request_params_->wall_clock_based_wait_enabled()
            << ", Update Check Count Wait Enabled = "
            << omaha_request_params_->update_check_count_wait_enabled()
            << ", Waiting Period = " << utils::FormatSecs(
               omaha_request_params_->waiting_period().InSeconds());

  LOG(INFO) << "Use p2p For Downloading = "
            << omaha_request_params_->use_p2p_for_downloading()
            << ", Use p2p For Sharing = "
            << omaha_request_params_->use_p2p_for_sharing();

  obeying_proxies_ = true;
  if (obey_proxies || proxy_manual_checks_ == 0) {
    LOG(INFO) << "forced to obey proxies";
    // If forced to obey proxies, every 20th request will not use proxies
    proxy_manual_checks_++;
    LOG(INFO) << "proxy manual checks: " << proxy_manual_checks_;
    if (proxy_manual_checks_ >= kMaxConsecutiveObeyProxyRequests) {
      proxy_manual_checks_ = 0;
      obeying_proxies_ = false;
    }
  } else if (base::RandInt(0, 4) == 0) {
    obeying_proxies_ = false;
  }
  LOG_IF(INFO, !obeying_proxies_) << "To help ensure updates work, this update "
      "check we are ignoring the proxy settings and using "
      "direct connections.";

  DisableDeltaUpdateIfNeeded();
  return true;
}

void UpdateAttempter::CalculateScatteringParams(bool interactive) {
  // Take a copy of the old scatter value before we update it, as
  // we need to update the waiting period if this value changes.
  TimeDelta old_scatter_factor = scatter_factor_;
  const policy::DevicePolicy* device_policy = system_state_->device_policy();
  if (device_policy) {
    int64_t new_scatter_factor_in_secs = 0;
    device_policy->GetScatterFactorInSeconds(&new_scatter_factor_in_secs);
    if (new_scatter_factor_in_secs < 0)  // sanitize input, just in case.
      new_scatter_factor_in_secs  = 0;
    scatter_factor_ = TimeDelta::FromSeconds(new_scatter_factor_in_secs);
  }

  bool is_scatter_enabled = false;
  if (scatter_factor_.InSeconds() == 0) {
    LOG(INFO) << "Scattering disabled since scatter factor is set to 0";
  } else if (interactive) {
    LOG(INFO) << "Scattering disabled as this is an interactive update check";
  } else if (!system_state_->hardware()->IsOOBEComplete(nullptr)) {
    LOG(INFO) << "Scattering disabled since OOBE is not complete yet";
  } else {
    is_scatter_enabled = true;
    LOG(INFO) << "Scattering is enabled";
  }

  if (is_scatter_enabled) {
    // This means the scattering policy is turned on.
    // Now check if we need to update the waiting period. The two cases
    // in which we'd need to update the waiting period are:
    // 1. First time in process or a scheduled check after a user-initiated one.
    //    (omaha_request_params_->waiting_period will be zero in this case).
    // 2. Admin has changed the scattering policy value.
    //    (new scattering value will be different from old one in this case).
    int64_t wait_period_in_secs = 0;
    if (omaha_request_params_->waiting_period().InSeconds() == 0) {
      // First case. Check if we have a suitable value to set for
      // the waiting period.
      if (prefs_->GetInt64(kPrefsWallClockWaitPeriod, &wait_period_in_secs) &&
          wait_period_in_secs > 0 &&
          wait_period_in_secs <= scatter_factor_.InSeconds()) {
        // This means:
        // 1. There's a persisted value for the waiting period available.
        // 2. And that persisted value is still valid.
        // So, in this case, we should reuse the persisted value instead of
        // generating a new random value to improve the chances of a good
        // distribution for scattering.
        omaha_request_params_->set_waiting_period(
          TimeDelta::FromSeconds(wait_period_in_secs));
        LOG(INFO) << "Using persisted wall-clock waiting period: " <<
            utils::FormatSecs(
                omaha_request_params_->waiting_period().InSeconds());
      } else {
        // This means there's no persisted value for the waiting period
        // available or its value is invalid given the new scatter_factor value.
        // So, we should go ahead and regenerate a new value for the
        // waiting period.
        LOG(INFO) << "Persisted value not present or not valid ("
                  << utils::FormatSecs(wait_period_in_secs)
                  << ") for wall-clock waiting period.";
        GenerateNewWaitingPeriod();
      }
    } else if (scatter_factor_ != old_scatter_factor) {
      // This means there's already a waiting period value, but we detected
      // a change in the scattering policy value. So, we should regenerate the
      // waiting period to make sure it's within the bounds of the new scatter
      // factor value.
      GenerateNewWaitingPeriod();
    } else {
      // Neither the first time scattering is enabled nor the scattering value
      // changed. Nothing to do.
      LOG(INFO) << "Keeping current wall-clock waiting period: " <<
          utils::FormatSecs(
              omaha_request_params_->waiting_period().InSeconds());
    }

    // The invariant at this point is that omaha_request_params_->waiting_period
    // is non-zero no matter which path we took above.
    LOG_IF(ERROR, omaha_request_params_->waiting_period().InSeconds() == 0)
        << "Waiting Period should NOT be zero at this point!!!";

    // Since scattering is enabled, wall clock based wait will always be
    // enabled.
    omaha_request_params_->set_wall_clock_based_wait_enabled(true);

    // If we don't have any issues in accessing the file system to update
    // the update check count value, we'll turn that on as well.
    bool decrement_succeeded = DecrementUpdateCheckCount();
    omaha_request_params_->set_update_check_count_wait_enabled(
      decrement_succeeded);
  } else {
    // This means the scattering feature is turned off or disabled for
    // this particular update check. Make sure to disable
    // all the knobs and artifacts so that we don't invoke any scattering
    // related code.
    omaha_request_params_->set_wall_clock_based_wait_enabled(false);
    omaha_request_params_->set_update_check_count_wait_enabled(false);
    omaha_request_params_->set_waiting_period(TimeDelta::FromSeconds(0));
    prefs_->Delete(kPrefsWallClockWaitPeriod);
    prefs_->Delete(kPrefsUpdateCheckCount);
    // Don't delete the UpdateFirstSeenAt file as we don't want manual checks
    // that result in no-updates (e.g. due to server side throttling) to
    // cause update starvation by having the client generate a new
    // UpdateFirstSeenAt for each scheduled check that follows a manual check.
  }
}

void UpdateAttempter::GenerateNewWaitingPeriod() {
  omaha_request_params_->set_waiting_period(TimeDelta::FromSeconds(
      base::RandInt(1, scatter_factor_.InSeconds())));

  LOG(INFO) << "Generated new wall-clock waiting period: " << utils::FormatSecs(
                omaha_request_params_->waiting_period().InSeconds());

  // Do a best-effort to persist this in all cases. Even if the persistence
  // fails, we'll still be able to scatter based on our in-memory value.
  // The persistence only helps in ensuring a good overall distribution
  // across multiple devices if they tend to reboot too often.
  system_state_->payload_state()->SetScatteringWaitPeriod(
      omaha_request_params_->waiting_period());
}

void UpdateAttempter::BuildPostInstallActions(
    InstallPlanAction* previous_action) {
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action(
        new PostinstallRunnerAction());
  actions_.push_back(shared_ptr<AbstractAction>(postinstall_runner_action));
  BondActions(previous_action,
              postinstall_runner_action.get());
}

void UpdateAttempter::BuildUpdateActions(bool interactive) {
  CHECK(!processor_->IsRunning());
  processor_->set_delegate(this);

  // Actions:
  LibcurlHttpFetcher* update_check_fetcher =
      new LibcurlHttpFetcher(GetProxyResolver(), system_state_);
  // Try harder to connect to the network, esp when not interactive.
  // See comment in libcurl_http_fetcher.cc.
  update_check_fetcher->set_no_network_max_retries(interactive ? 1 : 3);
  update_check_fetcher->set_check_certificate(CertificateChecker::kUpdate);
  shared_ptr<OmahaRequestAction> update_check_action(
      new OmahaRequestAction(system_state_,
                             nullptr,
                             update_check_fetcher,  // passes ownership
                             false));
  shared_ptr<OmahaResponseHandlerAction> response_handler_action(
      new OmahaResponseHandlerAction(system_state_));
  // We start with the kernel so it's marked as invalid more quickly.
  shared_ptr<FilesystemCopierAction> kernel_filesystem_copier_action(
      new FilesystemCopierAction(system_state_, true, false));
  shared_ptr<FilesystemCopierAction> filesystem_copier_action(
      new FilesystemCopierAction(system_state_, false, false));

  shared_ptr<OmahaRequestAction> download_started_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadStarted),
                             new LibcurlHttpFetcher(GetProxyResolver(),
                                                    system_state_),
                             false));
  LibcurlHttpFetcher* download_fetcher =
      new LibcurlHttpFetcher(GetProxyResolver(), system_state_);
  download_fetcher->set_check_certificate(CertificateChecker::kDownload);
  shared_ptr<DownloadAction> download_action(
      new DownloadAction(prefs_,
                         system_state_,
                         new MultiRangeHttpFetcher(
                             download_fetcher)));  // passes ownership
  shared_ptr<OmahaRequestAction> download_finished_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadFinished),
                             new LibcurlHttpFetcher(GetProxyResolver(),
                                                    system_state_),
                             false));
  shared_ptr<FilesystemCopierAction> filesystem_verifier_action(
      new FilesystemCopierAction(system_state_, false, true));
  shared_ptr<FilesystemCopierAction> kernel_filesystem_verifier_action(
      new FilesystemCopierAction(system_state_, true, true));
  shared_ptr<OmahaRequestAction> update_complete_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(OmahaEvent::kTypeUpdateComplete),
                             new LibcurlHttpFetcher(GetProxyResolver(),
                                                    system_state_),
                             false));

  download_action->set_delegate(this);
  response_handler_action_ = response_handler_action;
  download_action_ = download_action;

  actions_.push_back(shared_ptr<AbstractAction>(update_check_action));
  actions_.push_back(shared_ptr<AbstractAction>(response_handler_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      kernel_filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_started_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_finished_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_verifier_action));
    actions_.push_back(shared_ptr<AbstractAction>(
        kernel_filesystem_verifier_action));

  // Bond them together. We have to use the leaf-types when calling
  // BondActions().
  BondActions(update_check_action.get(),
              response_handler_action.get());
  BondActions(response_handler_action.get(),
              filesystem_copier_action.get());
  BondActions(filesystem_copier_action.get(),
              kernel_filesystem_copier_action.get());
  BondActions(kernel_filesystem_copier_action.get(),
              download_action.get());
  BondActions(download_action.get(),
              filesystem_verifier_action.get());
  BondActions(filesystem_verifier_action.get(),
              kernel_filesystem_verifier_action.get());

  BuildPostInstallActions(kernel_filesystem_verifier_action.get());

  actions_.push_back(shared_ptr<AbstractAction>(update_complete_action));

  // Enqueue the actions
  for (vector<shared_ptr<AbstractAction>>::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    processor_->EnqueueAction(it->get());
  }
}

bool UpdateAttempter::Rollback(bool powerwash) {
  if (!CanRollback()) {
    return false;
  }

  // Extra check for enterprise-enrolled devices since they don't support
  // powerwash.
  if (powerwash) {
    // Enterprise-enrolled devices have an empty owner in their device policy.
    string owner;
    RefreshDevicePolicy();
    const policy::DevicePolicy* device_policy = system_state_->device_policy();
    if (device_policy && (!device_policy->GetOwner(&owner) || owner.empty())) {
      LOG(ERROR) << "Enterprise device detected. "
                 << "Cannot perform a powerwash for enterprise devices.";
      return false;
    }
  }

  processor_->set_delegate(this);

  // Initialize the default request params.
  if (!omaha_request_params_->Init("", "", true)) {
    LOG(ERROR) << "Unable to initialize Omaha request params.";
    return false;
  }

  LOG(INFO) << "Setting rollback options.";
  InstallPlan install_plan;

  TEST_AND_RETURN_FALSE(utils::GetInstallDev(
      system_state_->hardware()->BootDevice(),
      &install_plan.install_path));

  install_plan.kernel_install_path =
      utils::KernelDeviceOfBootDevice(install_plan.install_path);
  install_plan.powerwash_required = powerwash;

  LOG(INFO) << "Using this install plan:";
  install_plan.Dump();

  shared_ptr<InstallPlanAction> install_plan_action(
      new InstallPlanAction(install_plan));
  actions_.push_back(shared_ptr<AbstractAction>(install_plan_action));

  BuildPostInstallActions(install_plan_action.get());

  // Enqueue the actions
  for (vector<shared_ptr<AbstractAction>>::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    processor_->EnqueueAction(it->get());
  }

  // Update the payload state for Rollback.
  system_state_->payload_state()->Rollback();

  SetStatusAndNotify(UPDATE_STATUS_ATTEMPTING_ROLLBACK);

  // Just in case we didn't update boot flags yet, make sure they're updated
  // before any update processing starts. This also schedules the start of the
  // actions we just posted.
  start_action_processor_ = true;
  UpdateBootFlags();
  return true;
}

bool UpdateAttempter::CanRollback() const {
  // We can only rollback if the update_engine isn't busy and we have a valid
  // rollback partition.
  return (status_ == UPDATE_STATUS_IDLE && !GetRollbackPartition().empty());
}

string UpdateAttempter::GetRollbackPartition() const {
  vector<string> kernel_devices =
      system_state_->hardware()->GetKernelDevices();

  string boot_kernel_device =
      system_state_->hardware()->BootKernelDevice();

  LOG(INFO) << "UpdateAttempter::GetRollbackPartition";
  for (const auto& name : kernel_devices)
    LOG(INFO) << "  Available kernel device = " << name;
  LOG(INFO) << "  Boot kernel device =      " << boot_kernel_device;

  auto current = std::find(kernel_devices.begin(), kernel_devices.end(),
                           boot_kernel_device);

  if (current == kernel_devices.end()) {
    LOG(ERROR) << "Unable to find the boot kernel device in the list of "
               << "available devices";
    return string();
  }

  for (string const& device_name : kernel_devices) {
    if (device_name != *current) {
      bool bootable = false;
      if (system_state_->hardware()->IsKernelBootable(device_name, &bootable) &&
          bootable) {
        return device_name;
      }
    }
  }

  return string();
}

vector<std::pair<string, bool>>
    UpdateAttempter::GetKernelDevices() const {
  vector<string> kernel_devices =
    system_state_->hardware()->GetKernelDevices();

  string boot_kernel_device =
    system_state_->hardware()->BootKernelDevice();

  vector<std::pair<string, bool>> info_list;
  info_list.reserve(kernel_devices.size());

  for (string device_name : kernel_devices) {
    bool bootable = false;
    system_state_->hardware()->IsKernelBootable(device_name, &bootable);
    // Add '*' to the name of the partition we booted from.
    if (device_name == boot_kernel_device)
      device_name += '*';
    info_list.emplace_back(device_name, bootable);
  }

  return info_list;
}

void UpdateAttempter::CheckForUpdate(const string& app_version,
                                     const string& omaha_url,
                                     bool interactive) {
  LOG(INFO) << "Forced update check requested.";
  forced_app_version_ = app_version;
  forced_omaha_url_ = omaha_url;
  if (forced_update_pending_callback_.get()) {
    // Make sure that a scheduling request is made prior to calling the forced
    // update pending callback.
    ScheduleUpdates();
    forced_update_pending_callback_->Run(true, interactive);
  }
}

bool UpdateAttempter::RebootIfNeeded() {
  if (status_ != UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    LOG(INFO) << "Reboot requested, but status is "
              << UpdateStatusToString(status_) << ", so not rebooting.";
    return false;
  }

  if (USE_POWER_MANAGEMENT && RequestPowerManagerReboot())
    return true;

  return RebootDirectly();
}

void UpdateAttempter::WriteUpdateCompletedMarker() {
  if (update_completed_marker_.empty())
    return;

  int64_t value = system_state_->clock()->GetBootTime().ToInternalValue();
  string contents = StringPrintf("%" PRIi64, value);

  utils::WriteFile(update_completed_marker_.c_str(),
                   contents.c_str(),
                   contents.length());
}

bool UpdateAttempter::RequestPowerManagerReboot() {
  GError* error = nullptr;
  DBusGConnection* bus = dbus_iface_->BusGet(DBUS_BUS_SYSTEM, &error);
  if (!bus) {
    LOG(ERROR) << "Failed to get system bus: "
               << utils::GetAndFreeGError(&error);
    return false;
  }

  LOG(INFO) << "Calling " << power_manager::kPowerManagerInterface << "."
            << power_manager::kRequestRestartMethod;
  DBusGProxy* proxy = dbus_iface_->ProxyNewForName(
      bus,
      power_manager::kPowerManagerServiceName,
      power_manager::kPowerManagerServicePath,
      power_manager::kPowerManagerInterface);
  const gboolean success = dbus_iface_->ProxyCall_1_0(
      proxy,
      power_manager::kRequestRestartMethod,
      &error,
      power_manager::REQUEST_RESTART_FOR_UPDATE);
  dbus_iface_->ProxyUnref(proxy);

  if (!success) {
    LOG(ERROR) << "Failed to call " << power_manager::kRequestRestartMethod
               << ": " << utils::GetAndFreeGError(&error);
    return false;
  }

  return true;
}

bool UpdateAttempter::RebootDirectly() {
  vector<string> command;
  command.push_back("/sbin/shutdown");
  command.push_back("-r");
  command.push_back("now");
  LOG(INFO) << "Running \"" << JoinString(command, ' ') << "\"";
  int rc = 0;
  Subprocess::SynchronousExec(command, &rc, nullptr);
  return rc == 0;
}

void UpdateAttempter::OnUpdateScheduled(EvalStatus status,
                                        const UpdateCheckParams& params) {
  waiting_for_scheduled_check_ = false;

  if (status == EvalStatus::kSucceeded) {
    if (!params.updates_enabled) {
      LOG(WARNING) << "Updates permanently disabled.";
      // Signal disabled status, then switch right back to idle. This is
      // necessary for ensuring that observers waiting for a signal change will
      // actually notice one on subsequent calls. Note that we don't need to
      // re-schedule a check in this case as updates are permanently disabled;
      // further (forced) checks may still initiate a scheduling call.
      SetStatusAndNotify(UPDATE_STATUS_DISABLED);
      SetStatusAndNotify(UPDATE_STATUS_IDLE);
      return;
    }

    LOG(INFO) << "Running "
              << (params.is_interactive ? "interactive" : "periodic")
              << " update.";

    string app_version, omaha_url;
    if (params.is_interactive) {
      app_version = forced_app_version_;
      omaha_url = forced_omaha_url_;
    } else {
      // Flush previously generated UMA reports before periodic updates.
      CertificateChecker::FlushReport();
    }

    Update(app_version, omaha_url, params.target_channel,
           params.target_version_prefix, false, params.is_interactive);
  } else {
    LOG(WARNING)
        << "Update check scheduling failed (possibly timed out); retrying.";
    ScheduleUpdates();
  }

  // This check ensures that future update checks will be or are already
  // scheduled. The check should never fail. A check failure means that there's
  // a bug that will most likely prevent further automatic update checks. It
  // seems better to crash in such cases and restart the update_engine daemon
  // into, hopefully, a known good state.
  CHECK(IsUpdateRunningOrScheduled());
}

void UpdateAttempter::UpdateLastCheckedTime() {
  last_checked_time_ = system_state_->clock()->GetWallclockTime().ToTimeT();
}

// Delegate methods:
void UpdateAttempter::ProcessingDone(const ActionProcessor* processor,
                                     ErrorCode code) {
  LOG(INFO) << "Processing Done.";
  actions_.clear();

  // Reset cpu shares back to normal.
  CleanupCpuSharesManagement();

  if (status_ == UPDATE_STATUS_REPORTING_ERROR_EVENT) {
    LOG(INFO) << "Error event sent.";

    // Inform scheduler of new status;
    SetStatusAndNotify(UPDATE_STATUS_IDLE);
    ScheduleUpdates();

    if (!fake_update_success_) {
      return;
    }
    LOG(INFO) << "Booted from FW B and tried to install new firmware, "
        "so requesting reboot from user.";
  }

  if (code == ErrorCode::kSuccess) {
    WriteUpdateCompletedMarker();
    prefs_->SetInt64(kPrefsDeltaUpdateFailures, 0);
    prefs_->SetString(kPrefsPreviousVersion,
                      omaha_request_params_->app_version());
    DeltaPerformer::ResetUpdateProgress(prefs_, false);

    system_state_->payload_state()->UpdateSucceeded();

    // Since we're done with scattering fully at this point, this is the
    // safest point delete the state files, as we're sure that the status is
    // set to reboot (which means no more updates will be applied until reboot)
    // This deletion is required for correctness as we want the next update
    // check to re-create a new random number for the update check count.
    // Similarly, we also delete the wall-clock-wait period that was persisted
    // so that we start with a new random value for the next update check
    // after reboot so that the same device is not favored or punished in any
    // way.
    prefs_->Delete(kPrefsUpdateCheckCount);
    system_state_->payload_state()->SetScatteringWaitPeriod(TimeDelta());
    prefs_->Delete(kPrefsUpdateFirstSeenAt);

    SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT);
    ScheduleUpdates();
    LOG(INFO) << "Update successfully applied, waiting to reboot.";

    // This pointer is null during rollback operations, and the stats
    // don't make much sense then anyway.
    if (response_handler_action_) {
      const InstallPlan& install_plan =
          response_handler_action_->install_plan();

      // Generate an unique payload identifier.
      const string target_version_uid =
          install_plan.payload_hash + ":" + install_plan.metadata_signature;

      // Expect to reboot into the new version to send the proper metric during
      // next boot.
      system_state_->payload_state()->ExpectRebootInNewVersion(
          target_version_uid);

      // Also report the success code so that the percentiles can be
      // interpreted properly for the remaining error codes in UMA.
      utils::SendErrorCodeToUma(system_state_, code);
    } else {
      // If we just finished a rollback, then we expect to have no Omaha
      // response. Otherwise, it's an error.
      if (system_state_->payload_state()->GetRollbackVersion().empty()) {
        LOG(ERROR) << "Can't send metrics because expected "
            "response_handler_action_ missing.";
      }
    }
    return;
  }

  if (ScheduleErrorEventAction()) {
    return;
  }
  LOG(INFO) << "No update.";
  SetStatusAndNotify(UPDATE_STATUS_IDLE);
  ScheduleUpdates();
}

void UpdateAttempter::ProcessingStopped(const ActionProcessor* processor) {
  // Reset cpu shares back to normal.
  CleanupCpuSharesManagement();
  download_progress_ = 0.0;
  SetStatusAndNotify(UPDATE_STATUS_IDLE);
  ScheduleUpdates();
  actions_.clear();
  error_event_.reset(nullptr);
}

// Called whenever an action has finished processing, either successfully
// or otherwise.
void UpdateAttempter::ActionCompleted(ActionProcessor* processor,
                                      AbstractAction* action,
                                      ErrorCode code) {
  // Reset download progress regardless of whether or not the download
  // action succeeded. Also, get the response code from HTTP request
  // actions (update download as well as the initial update check
  // actions).
  const string type = action->Type();
  if (type == DownloadAction::StaticType()) {
    download_progress_ = 0.0;
    DownloadAction* download_action = static_cast<DownloadAction*>(action);
    http_response_code_ = download_action->GetHTTPResponseCode();
  } else if (type == OmahaRequestAction::StaticType()) {
    OmahaRequestAction* omaha_request_action =
        static_cast<OmahaRequestAction*>(action);
    // If the request is not an event, then it's the update-check.
    if (!omaha_request_action->IsEvent()) {
      http_response_code_ = omaha_request_action->GetHTTPResponseCode();

      // Record the number of consecutive failed update checks.
      if (http_response_code_ == kHttpResponseInternalServerError ||
          http_response_code_ == kHttpResponseServiceUnavailable) {
        consecutive_failed_update_checks_++;
      } else {
        consecutive_failed_update_checks_ = 0;
      }

      // Store the server-dictated poll interval, if any.
      server_dictated_poll_interval_ =
          std::max(0, omaha_request_action->GetOutputObject().poll_interval);
    }
  }
  if (code != ErrorCode::kSuccess) {
    // If the current state is at or past the download phase, count the failure
    // in case a switch to full update becomes necessary. Ignore network
    // transfer timeouts and failures.
    if (status_ >= UPDATE_STATUS_DOWNLOADING &&
        code != ErrorCode::kDownloadTransferError) {
      MarkDeltaUpdateFailure();
    }
    // On failure, schedule an error event to be sent to Omaha.
    CreatePendingErrorEvent(action, code);
    return;
  }
  // Find out which action completed.
  if (type == OmahaResponseHandlerAction::StaticType()) {
    // Note that the status will be updated to DOWNLOADING when some bytes get
    // actually downloaded from the server and the BytesReceived callback is
    // invoked. This avoids notifying the user that a download has started in
    // cases when the server and the client are unable to initiate the download.
    CHECK(action == response_handler_action_.get());
    const InstallPlan& plan = response_handler_action_->install_plan();
    UpdateLastCheckedTime();
    new_version_ = plan.version;
    new_payload_size_ = plan.payload_size;
    SetupDownload();
    SetupCpuSharesManagement();
    SetStatusAndNotify(UPDATE_STATUS_UPDATE_AVAILABLE);
  } else if (type == DownloadAction::StaticType()) {
    SetStatusAndNotify(UPDATE_STATUS_FINALIZING);
  }
}

// Stop updating. An attempt will be made to record status to the disk
// so that updates can be resumed later.
void UpdateAttempter::Terminate() {
  // TODO(adlr): implement this method.
  NOTIMPLEMENTED();
}

// Try to resume from a previously Terminate()d update.
void UpdateAttempter::ResumeUpdating() {
  // TODO(adlr): implement this method.
  NOTIMPLEMENTED();
}

void UpdateAttempter::SetDownloadStatus(bool active) {
  download_active_ = active;
  LOG(INFO) << "Download status: " << (active ? "active" : "inactive");
}

void UpdateAttempter::BytesReceived(uint64_t bytes_received, uint64_t total) {
  if (!download_active_) {
    LOG(ERROR) << "BytesReceived called while not downloading.";
    return;
  }
  double progress = static_cast<double>(bytes_received) /
      static_cast<double>(total);
  // Self throttle based on progress. Also send notifications if
  // progress is too slow.
  const double kDeltaPercent = 0.01;  // 1%
  if (status_ != UPDATE_STATUS_DOWNLOADING ||
      bytes_received == total ||
      progress - download_progress_ >= kDeltaPercent ||
      TimeTicks::Now() - last_notify_time_ >= TimeDelta::FromSeconds(10)) {
    download_progress_ = progress;
    SetStatusAndNotify(UPDATE_STATUS_DOWNLOADING);
  }
}

bool UpdateAttempter::ResetStatus() {
  LOG(INFO) << "Attempting to reset state from "
            << UpdateStatusToString(status_) << " to UPDATE_STATUS_IDLE";

  switch (status_) {
    case UPDATE_STATUS_IDLE:
      // no-op.
      return true;

    case UPDATE_STATUS_UPDATED_NEED_REBOOT:  {
      bool ret_value = true;
      status_ = UPDATE_STATUS_IDLE;
      LOG(INFO) << "Reset Successful";

      // Remove the reboot marker so that if the machine is rebooted
      // after resetting to idle state, it doesn't go back to
      // UPDATE_STATUS_UPDATED_NEED_REBOOT state.
      if (!update_completed_marker_.empty()) {
        if (!base::DeleteFile(base::FilePath(update_completed_marker_), false))
          ret_value = false;
      }

      // Notify the PayloadState that the successful payload was canceled.
      system_state_->payload_state()->ResetUpdateStatus();

      return ret_value;
    }

    default:
      LOG(ERROR) << "Reset not allowed in this state.";
      return false;
  }
}

bool UpdateAttempter::GetStatus(int64_t* last_checked_time,
                                double* progress,
                                string* current_operation,
                                string* new_version,
                                int64_t* new_payload_size) {
  *last_checked_time = last_checked_time_;
  *progress = download_progress_;
  *current_operation = UpdateStatusToString(status_);
  *new_version = new_version_;
  *new_payload_size = new_payload_size_;
  return true;
}

void UpdateAttempter::UpdateBootFlags() {
  if (update_boot_flags_running_) {
    LOG(INFO) << "Update boot flags running, nothing to do.";
    return;
  }
  if (updated_boot_flags_) {
    LOG(INFO) << "Already updated boot flags. Skipping.";
    if (start_action_processor_) {
      ScheduleProcessingStart();
    }
    return;
  }
  // This is purely best effort. Failures should be logged by Subprocess. Run
  // the script asynchronously to avoid blocking the event loop regardless of
  // the script runtime.
  update_boot_flags_running_ = true;
  LOG(INFO) << "Updating boot flags...";
  vector<string> cmd{set_good_kernel_cmd_};
  if (!Subprocess::Get().Exec(cmd, StaticCompleteUpdateBootFlags, this)) {
    CompleteUpdateBootFlags(1);
  }
}

void UpdateAttempter::CompleteUpdateBootFlags(int return_code) {
  update_boot_flags_running_ = false;
  updated_boot_flags_ = true;
  if (start_action_processor_) {
    ScheduleProcessingStart();
  }
}

void UpdateAttempter::StaticCompleteUpdateBootFlags(
    int return_code,
    const string& output,
    void* p) {
  reinterpret_cast<UpdateAttempter*>(p)->CompleteUpdateBootFlags(return_code);
}

void UpdateAttempter::BroadcastStatus() {
  if (!dbus_service_) {
    return;
  }
  last_notify_time_ = TimeTicks::Now();
  update_engine_service_emit_status_update(
      dbus_service_,
      last_checked_time_,
      download_progress_,
      UpdateStatusToString(status_),
      new_version_.c_str(),
      new_payload_size_);
}

uint32_t UpdateAttempter::GetErrorCodeFlags()  {
  uint32_t flags = 0;

  if (!system_state_->hardware()->IsNormalBootMode())
    flags |= static_cast<uint32_t>(ErrorCode::kDevModeFlag);

  if (response_handler_action_.get() &&
      response_handler_action_->install_plan().is_resume)
    flags |= static_cast<uint32_t>(ErrorCode::kResumedFlag);

  if (!system_state_->hardware()->IsOfficialBuild())
    flags |= static_cast<uint32_t>(ErrorCode::kTestImageFlag);

  if (omaha_request_params_->update_url() != kProductionOmahaUrl)
    flags |= static_cast<uint32_t>(ErrorCode::kTestOmahaUrlFlag);

  return flags;
}

bool UpdateAttempter::ShouldCancel(ErrorCode* cancel_reason) {
  // Check if the channel we're attempting to update to is the same as the
  // target channel currently chosen by the user.
  OmahaRequestParams* params = system_state_->request_params();
  if (params->download_channel() != params->target_channel()) {
    LOG(ERROR) << "Aborting download as target channel: "
               << params->target_channel()
               << " is different from the download channel: "
               << params->download_channel();
    *cancel_reason = ErrorCode::kUpdateCanceledByChannelChange;
    return true;
  }

  return false;
}

void UpdateAttempter::SetStatusAndNotify(UpdateStatus status) {
  status_ = status;
  BroadcastStatus();
}

void UpdateAttempter::CreatePendingErrorEvent(AbstractAction* action,
                                              ErrorCode code) {
  if (error_event_.get()) {
    // This shouldn't really happen.
    LOG(WARNING) << "There's already an existing pending error event.";
    return;
  }

  // For now assume that a generic Omaha response action failure means that
  // there's no update so don't send an event. Also, double check that the
  // failure has not occurred while sending an error event -- in which case
  // don't schedule another. This shouldn't really happen but just in case...
  if ((action->Type() == OmahaResponseHandlerAction::StaticType() &&
       code == ErrorCode::kError) ||
      status_ == UPDATE_STATUS_REPORTING_ERROR_EVENT) {
    return;
  }

  // Classify the code to generate the appropriate result so that
  // the Borgmon charts show up the results correctly.
  // Do this before calling GetErrorCodeForAction which could potentially
  // augment the bit representation of code and thus cause no matches for
  // the switch cases below.
  OmahaEvent::Result event_result;
  switch (code) {
    case ErrorCode::kOmahaUpdateIgnoredPerPolicy:
    case ErrorCode::kOmahaUpdateDeferredPerPolicy:
    case ErrorCode::kOmahaUpdateDeferredForBackoff:
      event_result = OmahaEvent::kResultUpdateDeferred;
      break;
    default:
      event_result = OmahaEvent::kResultError;
      break;
  }

  code = GetErrorCodeForAction(action, code);
  fake_update_success_ = code == ErrorCode::kPostinstallBootedFromFirmwareB;

  // Compute the final error code with all the bit flags to be sent to Omaha.
  code = static_cast<ErrorCode>(
      static_cast<uint32_t>(code) | GetErrorCodeFlags());
  error_event_.reset(new OmahaEvent(OmahaEvent::kTypeUpdateComplete,
                                    event_result,
                                    code));
}

bool UpdateAttempter::ScheduleErrorEventAction() {
  if (error_event_.get() == nullptr)
    return false;

  LOG(ERROR) << "Update failed.";
  system_state_->payload_state()->UpdateFailed(error_event_->error_code);

  // Send it to Uma.
  LOG(INFO) << "Reporting the error event";
  utils::SendErrorCodeToUma(system_state_, error_event_->error_code);

  // Send it to Omaha.
  shared_ptr<OmahaRequestAction> error_event_action(
      new OmahaRequestAction(system_state_,
                             error_event_.release(),  // Pass ownership.
                             new LibcurlHttpFetcher(GetProxyResolver(),
                                                    system_state_),
                             false));
  actions_.push_back(shared_ptr<AbstractAction>(error_event_action));
  processor_->EnqueueAction(error_event_action.get());
  SetStatusAndNotify(UPDATE_STATUS_REPORTING_ERROR_EVENT);
  processor_->StartProcessing();
  return true;
}

void UpdateAttempter::SetCpuShares(utils::CpuShares shares) {
  if (shares_ == shares) {
    return;
  }
  if (utils::SetCpuShares(shares)) {
    shares_ = shares;
    LOG(INFO) << "CPU shares = " << shares_;
  }
}

void UpdateAttempter::SetupCpuSharesManagement() {
  if (manage_shares_source_) {
    LOG(ERROR) << "Cpu shares timeout source hasn't been destroyed.";
    CleanupCpuSharesManagement();
  }
  const int kCpuSharesTimeout = 2 * 60 * 60;  // 2 hours
  manage_shares_source_ = g_timeout_source_new_seconds(kCpuSharesTimeout);
  g_source_set_callback(manage_shares_source_,
                        StaticManageCpuSharesCallback,
                        this,
                        nullptr);
  g_source_attach(manage_shares_source_, nullptr);
  SetCpuShares(utils::kCpuSharesLow);
}

void UpdateAttempter::CleanupCpuSharesManagement() {
  if (manage_shares_source_) {
    g_source_destroy(manage_shares_source_);
    manage_shares_source_ = nullptr;
  }
  SetCpuShares(utils::kCpuSharesNormal);
}

gboolean UpdateAttempter::StaticManageCpuSharesCallback(gpointer data) {
  return reinterpret_cast<UpdateAttempter*>(data)->ManageCpuSharesCallback();
}

gboolean UpdateAttempter::StaticStartProcessing(gpointer data) {
  reinterpret_cast<UpdateAttempter*>(data)->processor_->StartProcessing();
  return FALSE;  // Don't call this callback again.
}

void UpdateAttempter::ScheduleProcessingStart() {
  LOG(INFO) << "Scheduling an action processor start.";
  start_action_processor_ = false;
  g_idle_add(&StaticStartProcessing, this);
}

bool UpdateAttempter::ManageCpuSharesCallback() {
  SetCpuShares(utils::kCpuSharesNormal);
  manage_shares_source_ = nullptr;
  return false;  // Destroy the timeout source.
}

void UpdateAttempter::DisableDeltaUpdateIfNeeded() {
  int64_t delta_failures;
  if (omaha_request_params_->delta_okay() &&
      prefs_->GetInt64(kPrefsDeltaUpdateFailures, &delta_failures) &&
      delta_failures >= kMaxDeltaUpdateFailures) {
    LOG(WARNING) << "Too many delta update failures, forcing full update.";
    omaha_request_params_->set_delta_okay(false);
  }
}

void UpdateAttempter::MarkDeltaUpdateFailure() {
  // Don't try to resume a failed delta update.
  DeltaPerformer::ResetUpdateProgress(prefs_, false);
  int64_t delta_failures;
  if (!prefs_->GetInt64(kPrefsDeltaUpdateFailures, &delta_failures) ||
      delta_failures < 0) {
    delta_failures = 0;
  }
  prefs_->SetInt64(kPrefsDeltaUpdateFailures, ++delta_failures);
}

void UpdateAttempter::SetupDownload() {
  MultiRangeHttpFetcher* fetcher =
      static_cast<MultiRangeHttpFetcher*>(download_action_->http_fetcher());
  fetcher->ClearRanges();
  if (response_handler_action_->install_plan().is_resume) {
    // Resuming an update so fetch the update manifest metadata first.
    int64_t manifest_metadata_size = 0;
    prefs_->GetInt64(kPrefsManifestMetadataSize, &manifest_metadata_size);
    fetcher->AddRange(0, manifest_metadata_size);
    // If there're remaining unprocessed data blobs, fetch them. Be careful not
    // to request data beyond the end of the payload to avoid 416 HTTP response
    // error codes.
    int64_t next_data_offset = 0;
    prefs_->GetInt64(kPrefsUpdateStateNextDataOffset, &next_data_offset);
    uint64_t resume_offset = manifest_metadata_size + next_data_offset;
    if (resume_offset < response_handler_action_->install_plan().payload_size) {
      fetcher->AddRange(resume_offset);
    }
  } else {
    fetcher->AddRange(0);
  }
}

void UpdateAttempter::PingOmaha() {
  if (!processor_->IsRunning()) {
    shared_ptr<OmahaRequestAction> ping_action(
        new OmahaRequestAction(system_state_,
                               nullptr,
                               new LibcurlHttpFetcher(GetProxyResolver(),
                                                      system_state_),
                               true));
    actions_.push_back(shared_ptr<OmahaRequestAction>(ping_action));
    processor_->set_delegate(nullptr);
    processor_->EnqueueAction(ping_action.get());
    // Call StartProcessing() synchronously here to avoid any race conditions
    // caused by multiple outstanding ping Omaha requests.  If we call
    // StartProcessing() asynchronously, the device can be suspended before we
    // get a chance to callback to StartProcessing().  When the device resumes
    // (assuming the device sleeps longer than the next update check period),
    // StartProcessing() is called back and at the same time, the next update
    // check is fired which eventually invokes StartProcessing().  A crash
    // can occur because StartProcessing() checks to make sure that the
    // processor is idle which it isn't due to the two concurrent ping Omaha
    // requests.
    processor_->StartProcessing();
  } else {
    LOG(WARNING) << "Action processor running, Omaha ping suppressed.";
  }

  // Update the last check time here; it may be re-updated when an Omaha
  // response is received, but this will prevent us from repeatedly scheduling
  // checks in the case where a response is not received.
  UpdateLastCheckedTime();

  // Update the status which will schedule the next update check
  SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT);
  ScheduleUpdates();
}


bool UpdateAttempter::DecrementUpdateCheckCount() {
  int64_t update_check_count_value;

  if (!prefs_->Exists(kPrefsUpdateCheckCount)) {
    // This file does not exist. This means we haven't started our update
    // check count down yet, so nothing more to do. This file will be created
    // later when we first satisfy the wall-clock-based-wait period.
    LOG(INFO) << "No existing update check count. That's normal.";
    return true;
  }

  if (prefs_->GetInt64(kPrefsUpdateCheckCount, &update_check_count_value)) {
    // Only if we're able to read a proper integer value, then go ahead
    // and decrement and write back the result in the same file, if needed.
    LOG(INFO) << "Update check count = " << update_check_count_value;

    if (update_check_count_value == 0) {
      // It could be 0, if, for some reason, the file didn't get deleted
      // when we set our status to waiting for reboot. so we just leave it
      // as is so that we can prevent another update_check wait for this client.
      LOG(INFO) << "Not decrementing update check count as it's already 0.";
      return true;
    }

    if (update_check_count_value > 0)
      update_check_count_value--;
    else
      update_check_count_value = 0;

    // Write out the new value of update_check_count_value.
    if (prefs_->SetInt64(kPrefsUpdateCheckCount, update_check_count_value)) {
      // We successfully wrote out te new value, so enable the
      // update check based wait.
      LOG(INFO) << "New update check count = " << update_check_count_value;
      return true;
    }
  }

  LOG(INFO) << "Deleting update check count state due to read/write errors.";

  // We cannot read/write to the file, so disable the update check based wait
  // so that we don't get stuck in this OS version by any chance (which could
  // happen if there's some bug that causes to read/write incorrectly).
  // Also attempt to delete the file to do our best effort to cleanup.
  prefs_->Delete(kPrefsUpdateCheckCount);
  return false;
}


void UpdateAttempter::UpdateEngineStarted() {
  // If we just booted into a new update, keep the previous OS version
  // in case we rebooted because of a crash of the old version, so we
  // can do a proper crash report with correct information.
  // This must be done before calling
  // system_state_->payload_state()->UpdateEngineStarted() since it will
  // delete SystemUpdated marker file.
  if (system_state_->system_rebooted() &&
      prefs_->Exists(kPrefsSystemUpdatedMarker)) {
    if (!prefs_->GetString(kPrefsPreviousVersion, &prev_version_)) {
      // If we fail to get the version string, make sure it stays empty.
      prev_version_.clear();
    }
  }

  system_state_->payload_state()->UpdateEngineStarted();
  StartP2PAtStartup();
}

bool UpdateAttempter::StartP2PAtStartup() {
  if (system_state_ == nullptr ||
      !system_state_->p2p_manager()->IsP2PEnabled()) {
    LOG(INFO) << "Not starting p2p at startup since it's not enabled.";
    return false;
  }

  if (system_state_->p2p_manager()->CountSharedFiles() < 1) {
    LOG(INFO) << "Not starting p2p at startup since our application "
              << "is not sharing any files.";
    return false;
  }

  return StartP2PAndPerformHousekeeping();
}

bool UpdateAttempter::StartP2PAndPerformHousekeeping() {
  if (system_state_ == nullptr)
    return false;

  if (!system_state_->p2p_manager()->IsP2PEnabled()) {
    LOG(INFO) << "Not starting p2p since it's not enabled.";
    return false;
  }

  LOG(INFO) << "Ensuring that p2p is running.";
  if (!system_state_->p2p_manager()->EnsureP2PRunning()) {
    LOG(ERROR) << "Error starting p2p.";
    return false;
  }

  LOG(INFO) << "Performing p2p housekeeping.";
  if (!system_state_->p2p_manager()->PerformHousekeeping()) {
    LOG(ERROR) << "Error performing housekeeping for p2p.";
    return false;
  }

  LOG(INFO) << "Done performing p2p housekeeping.";
  return true;
}

bool UpdateAttempter::GetBootTimeAtUpdate(Time *out_boot_time) {
  if (update_completed_marker_.empty())
    return false;

  string contents;
  if (!utils::ReadFile(update_completed_marker_, &contents))
    return false;

  char *endp;
  int64_t stored_value = strtoll(contents.c_str(), &endp, 10);
  if (*endp != '\0') {
    LOG(ERROR) << "Error parsing file " << update_completed_marker_ << " "
               << "with content '" << contents << "'";
    return false;
  }

  *out_boot_time = Time::FromInternalValue(stored_value);
  return true;
}

bool UpdateAttempter::IsUpdateRunningOrScheduled() {
  return ((status_ != UPDATE_STATUS_IDLE &&
           status_ != UPDATE_STATUS_UPDATED_NEED_REBOOT) ||
          waiting_for_scheduled_check_);
}

}  // namespace chromeos_update_engine
