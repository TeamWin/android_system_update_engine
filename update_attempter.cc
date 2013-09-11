// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_attempter.h"

#include <string>
#include <tr1/memory>
#include <vector>

#include <base/file_util.h>
#include <base/rand_util.h>
#include <glib.h>
#include <metrics/metrics_library.h>
#include <policy/libpolicy.h>
#include <policy/device_policy.h>

#include "update_engine/certificate_checker.h"
#include "update_engine/constants.h"
#include "update_engine/dbus_service.h"
#include "update_engine/download_action.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/gpio_handler.h"
#include "update_engine/hardware_interface.h"
#include "update_engine/libcurl_http_fetcher.h"
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
#include "update_engine/update_check_scheduler.h"

using base::TimeDelta;
using base::TimeTicks;
using google::protobuf::NewPermanentCallback;
using std::make_pair;
using std::tr1::shared_ptr;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

const int UpdateAttempter::kMaxDeltaUpdateFailures = 3;

// Private test server URL w/ custom port number.
// TODO(garnold) This is a temporary hack to allow us to test the closed loop
// automated update testing. To be replaced with an hard-coded local IP address.
const char* const UpdateAttempter::kTestUpdateUrl(
    "http://garnold.mtv.corp.google.com:8080/update");

namespace {
const int kMaxConsecutiveObeyProxyRequests = 20;

const char* kUpdateCompletedMarker =
    "/var/run/update_engine_autoupdate_completed";
}  // namespace {}

const char* UpdateStatusToString(UpdateStatus status) {
  switch (status) {
    case UPDATE_STATUS_IDLE:
      return "UPDATE_STATUS_IDLE";
    case UPDATE_STATUS_CHECKING_FOR_UPDATE:
      return "UPDATE_STATUS_CHECKING_FOR_UPDATE";
    case UPDATE_STATUS_UPDATE_AVAILABLE:
      return "UPDATE_STATUS_UPDATE_AVAILABLE";
    case UPDATE_STATUS_DOWNLOADING:
      return "UPDATE_STATUS_DOWNLOADING";
    case UPDATE_STATUS_VERIFYING:
      return "UPDATE_STATUS_VERIFYING";
    case UPDATE_STATUS_FINALIZING:
      return "UPDATE_STATUS_FINALIZING";
    case UPDATE_STATUS_UPDATED_NEED_REBOOT:
      return "UPDATE_STATUS_UPDATED_NEED_REBOOT";
    case UPDATE_STATUS_REPORTING_ERROR_EVENT:
      return "UPDATE_STATUS_REPORTING_ERROR_EVENT";
    case UPDATE_STATUS_ATTEMPTING_ROLLBACK:
      return "UPDATE_STATUS_ATTEMPTING_ROLLBACK";
    default:
      return "unknown status";
  }
}

// Turns a generic kErrorCodeError to a generic error code specific
// to |action| (e.g., kErrorCodeFilesystemCopierError). If |code| is
// not kErrorCodeError, or the action is not matched, returns |code|
// unchanged.
ErrorCode GetErrorCodeForAction(AbstractAction* action,
                                     ErrorCode code) {
  if (code != kErrorCodeError)
    return code;

  const string type = action->Type();
  if (type == OmahaRequestAction::StaticType())
    return kErrorCodeOmahaRequestError;
  if (type == OmahaResponseHandlerAction::StaticType())
    return kErrorCodeOmahaResponseHandlerError;
  if (type == FilesystemCopierAction::StaticType())
    return kErrorCodeFilesystemCopierError;
  if (type == PostinstallRunnerAction::StaticType())
    return kErrorCodePostinstallRunnerError;

  return code;
}

UpdateAttempter::UpdateAttempter(SystemState* system_state,
                                 DbusGlibInterface* dbus_iface)
    : chrome_proxy_resolver_(dbus_iface) {
  Init(system_state, kUpdateCompletedMarker);
}

UpdateAttempter::UpdateAttempter(SystemState* system_state,
                                 DbusGlibInterface* dbus_iface,
                                 const std::string& update_completed_marker)
    : chrome_proxy_resolver_(dbus_iface) {
  Init(system_state, update_completed_marker);
}


void UpdateAttempter::Init(SystemState* system_state,
                           const std::string& update_completed_marker) {
  // Initialite data members.
  processor_.reset(new ActionProcessor());
  system_state_ = system_state;
  dbus_service_ = NULL;
  update_check_scheduler_ = NULL;
  fake_update_success_ = false;
  http_response_code_ = 0;
  shares_ = utils::kCpuSharesNormal;
  manage_shares_source_ = NULL;
  download_active_ = false;
  download_progress_ = 0.0;
  last_checked_time_ = 0;
  new_version_ = "0.0.0.0";
  new_payload_size_ = 0;
  proxy_manual_checks_ = 0;
  obeying_proxies_ = true;
  updated_boot_flags_ = false;
  update_boot_flags_running_ = false;
  start_action_processor_ = false;
  is_using_test_url_ = false;
  is_test_mode_ = false;
  is_test_update_attempted_ = false;
  update_completed_marker_ = update_completed_marker;

  prefs_ = system_state->prefs();
  omaha_request_params_ = system_state->request_params();

  if (!update_completed_marker_.empty() &&
      utils::FileExists(update_completed_marker_.c_str()))
    status_ = UPDATE_STATUS_UPDATED_NEED_REBOOT;
  else
    status_ = UPDATE_STATUS_IDLE;
}

UpdateAttempter::~UpdateAttempter() {
  CleanupCpuSharesManagement();
}

void UpdateAttempter::Update(const string& app_version,
                             const string& omaha_url,
                             bool obey_proxies,
                             bool interactive,
                             bool is_test_mode) {
  chrome_proxy_resolver_.Init();
  fake_update_success_ = false;
  if (status_ == UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    // Although we have applied an update, we still want to ping Omaha
    // to ensure the number of active statistics is accurate.
    LOG(INFO) << "Not updating b/c we already updated and we're waiting for "
              << "reboot, we'll ping Omaha instead";
    PingOmaha();
    return;
  }
  if (status_ != UPDATE_STATUS_IDLE) {
    // Update in progress. Do nothing
    return;
  }

  if (!CalculateUpdateParams(app_version,
                             omaha_url,
                             obey_proxies,
                             interactive,
                             is_test_mode)) {
    return;
  }

  BuildUpdateActions(interactive);

  SetStatusAndNotify(UPDATE_STATUS_CHECKING_FOR_UPDATE,
                     kUpdateNoticeUnspecified);

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

  const policy::DevicePolicy* device_policy = NULL;
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

  if (system_state_ != NULL) {
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
                                            bool obey_proxies,
                                            bool interactive,
                                            bool is_test_mode) {
  http_response_code_ = 0;

  // Set the test mode flag for the current update attempt.
  is_test_mode_ = is_test_mode;
  RefreshDevicePolicy();
  const policy::DevicePolicy* device_policy = system_state_->device_policy();
  if (device_policy) {
    bool update_disabled = false;
    if (device_policy->GetUpdateDisabled(&update_disabled))
      omaha_request_params_->set_update_disabled(update_disabled);

    string target_version_prefix;
    if (device_policy->GetTargetVersionPrefix(&target_version_prefix))
      omaha_request_params_->set_target_version_prefix(target_version_prefix);

    set<string> allowed_types;
    string allowed_types_str;
    if (device_policy->GetAllowedConnectionTypesForUpdate(&allowed_types)) {
      set<string>::const_iterator iter;
      for (iter = allowed_types.begin(); iter != allowed_types.end(); ++iter)
        allowed_types_str += *iter + " ";
    }

    LOG(INFO) << "Networks over which updates are allowed per policy : "
              << (allowed_types_str.empty() ? "all" : allowed_types_str);
  }

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

  // Determine whether an alternative test address should be used.
  string omaha_url_to_use = omaha_url;
  if ((is_using_test_url_ = (omaha_url_to_use.empty() && is_test_mode_))) {
    omaha_url_to_use = kTestUpdateUrl;
    LOG(INFO) << "using alternative server address: " << omaha_url_to_use;
  }

  if (!omaha_request_params_->Init(app_version,
                                   omaha_url_to_use,
                                   interactive)) {
    LOG(ERROR) << "Unable to initialize Omaha request params.";
    return false;
  }

  // Set the target channel iff ReleaseChannelDelegated policy is set to
  // false and a non-empty ReleaseChannel policy is present. If delegated
  // is true, we'll ignore ReleaseChannel policy value.
  if (device_policy) {
    bool delegated = false;
    if (!device_policy->GetReleaseChannelDelegated(&delegated) || delegated) {
      LOG(INFO) << "Channel settings are delegated to user by policy. "
                   "Ignoring ReleaseChannel policy value";
    }
    else {
      LOG(INFO) << "Channel settings are not delegated to the user by policy";
      string target_channel;
      if (device_policy->GetReleaseChannel(&target_channel) &&
          !target_channel.empty()) {
        // Pass in false for powerwash_allowed until we add it to the policy
        // protobuf.
        LOG(INFO) << "Setting target channel from ReleaseChannel policy value";
        omaha_request_params_->SetTargetChannel(target_channel, false);

        // Since this is the beginning of a new attempt, update the download
        // channel. The download channel won't be updated until the next
        // attempt, even if target channel changes meanwhile, so that how we'll
        // know if we should cancel the current download attempt if there's
        // such a change in target channel.
        omaha_request_params_->UpdateDownloadChannel();
      } else {
        LOG(INFO) << "No ReleaseChannel specified in policy";
      }
    }
  }

  LOG(INFO) << "update_disabled = "
            << utils::ToString(omaha_request_params_->update_disabled())
            << ", target_version_prefix = "
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
    int64 new_scatter_factor_in_secs = 0;
    device_policy->GetScatterFactorInSeconds(&new_scatter_factor_in_secs);
    if (new_scatter_factor_in_secs < 0) // sanitize input, just in case.
      new_scatter_factor_in_secs  = 0;
    scatter_factor_ = TimeDelta::FromSeconds(new_scatter_factor_in_secs);
  }

  bool is_scatter_enabled = false;
  if (scatter_factor_.InSeconds() == 0) {
    LOG(INFO) << "Scattering disabled since scatter factor is set to 0";
  } else if (interactive) {
    LOG(INFO) << "Scattering disabled as this is an interactive update check";
  } else if (!system_state_->IsOOBEComplete()) {
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
    int64 wait_period_in_secs = 0;
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
      }
      else {
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
  prefs_->SetInt64(kPrefsWallClockWaitPeriod,
                   omaha_request_params_->waiting_period().InSeconds());
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
      new LibcurlHttpFetcher(GetProxyResolver(), system_state_, is_test_mode_);
  // Try harder to connect to the network, esp when not interactive.
  // See comment in libcurl_http_fetcher.cc.
  update_check_fetcher->set_no_network_max_retries(interactive ? 1 : 3);
  update_check_fetcher->set_check_certificate(CertificateChecker::kUpdate);
  shared_ptr<OmahaRequestAction> update_check_action(
      new OmahaRequestAction(system_state_,
                             NULL,
                             update_check_fetcher,  // passes ownership
                             false));
  shared_ptr<OmahaResponseHandlerAction> response_handler_action(
      new OmahaResponseHandlerAction(system_state_));
  shared_ptr<FilesystemCopierAction> filesystem_copier_action(
      new FilesystemCopierAction(system_state_, false, false));
  shared_ptr<FilesystemCopierAction> kernel_filesystem_copier_action(
      new FilesystemCopierAction(system_state_, true, false));
  shared_ptr<OmahaRequestAction> download_started_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadStarted),
                             new LibcurlHttpFetcher(GetProxyResolver(),
                                                    system_state_,
                                                    is_test_mode_),
                             false));
  LibcurlHttpFetcher* download_fetcher =
      new LibcurlHttpFetcher(GetProxyResolver(), system_state_, is_test_mode_);
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
                                                    system_state_,
                                                    is_test_mode_),
                             false));
  shared_ptr<FilesystemCopierAction> filesystem_verifier_action(
      new FilesystemCopierAction(system_state_, false, true));
  shared_ptr<FilesystemCopierAction> kernel_filesystem_verifier_action(
      new FilesystemCopierAction(system_state_, true, true));
  shared_ptr<OmahaRequestAction> update_complete_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(OmahaEvent::kTypeUpdateComplete),
                             new LibcurlHttpFetcher(GetProxyResolver(),
                                                    system_state_,
                                                    is_test_mode_),
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
  for (vector<shared_ptr<AbstractAction> >::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    processor_->EnqueueAction(it->get());
  }
}

bool UpdateAttempter::Rollback(bool powerwash, string *install_path) {
  CHECK(!processor_->IsRunning());
  processor_->set_delegate(this);

  // TODO(sosa): crbug.com/252539 -- refactor channel into system_state and
  // check for != stable-channel here.
  RefreshDevicePolicy();

  // Initialize the default request params.
  if (!omaha_request_params_->Init("", "", true)) {
    LOG(ERROR) << "Unable to initialize Omaha request params.";
    return false;
  }

  if (omaha_request_params_->current_channel() == "stable-channel") {
    LOG(ERROR) << "Rollback is not supported while on the stable-channel.";
    return false;
  }

  LOG(INFO) << "Setting rollback options.";
  InstallPlan install_plan;
  if (install_path == NULL) {
    TEST_AND_RETURN_FALSE(utils::GetInstallDev(
        system_state_->hardware()->BootDevice(),
        &install_plan.install_path));
  }
  else {
    install_plan.install_path = *install_path;
  }

  install_plan.kernel_install_path =
      system_state_->hardware()->KernelDeviceOfBootDevice(
          install_plan.install_path);
  install_plan.powerwash_required = powerwash;
  if (powerwash) {
    // Enterprise-enrolled devices have an empty owner in their device policy.
    string owner;
    const policy::DevicePolicy* device_policy = system_state_->device_policy();
    if (!device_policy->GetOwner(&owner) || owner.empty()) {
      LOG(ERROR) << "Enterprise device detected. "
                 << "Cannot perform a powerwash for enterprise devices.";
      return false;
    }
  }

  LOG(INFO) << "Using this install plan:";
  install_plan.Dump();

  shared_ptr<InstallPlanAction> install_plan_action(
      new InstallPlanAction(install_plan));
  actions_.push_back(shared_ptr<AbstractAction>(install_plan_action));

  BuildPostInstallActions(install_plan_action.get());

  // Enqueue the actions
  for (vector<shared_ptr<AbstractAction> >::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    processor_->EnqueueAction(it->get());
  }

  // Update the payload state for Rollback.
  system_state_->payload_state()->Rollback();

  SetStatusAndNotify(UPDATE_STATUS_ATTEMPTING_ROLLBACK,
                     kUpdateNoticeUnspecified);

  // Just in case we didn't update boot flags yet, make sure they're updated
  // before any update processing starts. This also schedules the start of the
  // actions we just posted.
  start_action_processor_ = true;
  UpdateBootFlags();
  return true;
}

void UpdateAttempter::CheckForUpdate(const string& app_version,
                                     const string& omaha_url,
                                     bool interactive) {
  LOG(INFO) << "New update check requested";

  if (status_ != UPDATE_STATUS_IDLE) {
    LOG(INFO) << "Skipping update check because current status is "
              << UpdateStatusToString(status_);
    return;
  }

  // Read GPIO signals and determine whether this is an automated test scenario.
  // For safety, we only allow a test update to be performed once; subsequent
  // update requests will be carried out normally.
  bool is_test_mode = (!is_test_update_attempted_ &&
                       system_state_->gpio_handler()->IsTestModeSignaled());
  if (is_test_mode) {
    LOG(WARNING) << "this is a test mode update attempt!";
    is_test_update_attempted_ = true;
  }

  // Pass through the interactive flag, in case we want to simulate a scheduled
  // test.
  Update(app_version, omaha_url, true, interactive, is_test_mode);
}

bool UpdateAttempter::RebootIfNeeded() {
  if (status_ != UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    LOG(INFO) << "Reboot requested, but status is "
              << UpdateStatusToString(status_) << ", so not rebooting.";
    return false;
  }
  TEST_AND_RETURN_FALSE(utils::Reboot());
  return true;
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

    // Inform scheduler of new status; also specifically inform about a failed
    // update attempt with a test address.
    SetStatusAndNotify(UPDATE_STATUS_IDLE,
                       (is_using_test_url_ ? kUpdateNoticeTestAddrFailed :
                        kUpdateNoticeUnspecified));

    if (!fake_update_success_) {
      return;
    }
    LOG(INFO) << "Booted from FW B and tried to install new firmware, "
        "so requesting reboot from user.";
  }

  if (code == kErrorCodeSuccess) {
    if (!update_completed_marker_.empty())
      utils::WriteFile(update_completed_marker_.c_str(), "", 0);
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
    prefs_->Delete(kPrefsWallClockWaitPeriod);
    prefs_->Delete(kPrefsUpdateFirstSeenAt);

    SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT,
                       kUpdateNoticeUnspecified);
    LOG(INFO) << "Update successfully applied, waiting to reboot.";

    const InstallPlan& install_plan = response_handler_action_->install_plan();

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
    return;
  }

  if (ScheduleErrorEventAction()) {
    return;
  }
  LOG(INFO) << "No update.";
  SetStatusAndNotify(UPDATE_STATUS_IDLE, kUpdateNoticeUnspecified);
}

void UpdateAttempter::ProcessingStopped(const ActionProcessor* processor) {
  // Reset cpu shares back to normal.
  CleanupCpuSharesManagement();
  download_progress_ = 0.0;
  SetStatusAndNotify(UPDATE_STATUS_IDLE, kUpdateNoticeUnspecified);
  actions_.clear();
  error_event_.reset(NULL);
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
    DownloadAction* download_action = dynamic_cast<DownloadAction*>(action);
    http_response_code_ = download_action->GetHTTPResponseCode();
  } else if (type == OmahaRequestAction::StaticType()) {
    OmahaRequestAction* omaha_request_action =
        dynamic_cast<OmahaRequestAction*>(action);
    // If the request is not an event, then it's the update-check.
    if (!omaha_request_action->IsEvent()) {
      http_response_code_ = omaha_request_action->GetHTTPResponseCode();
      // Forward the server-dictated poll interval to the update check
      // scheduler, if any.
      if (update_check_scheduler_) {
        update_check_scheduler_->set_poll_interval(
            omaha_request_action->GetOutputObject().poll_interval);
      }
    }
  }
  if (code != kErrorCodeSuccess) {
    // If the current state is at or past the download phase, count the failure
    // in case a switch to full update becomes necessary. Ignore network
    // transfer timeouts and failures.
    if (status_ >= UPDATE_STATUS_DOWNLOADING &&
        code != kErrorCodeDownloadTransferError) {
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
    last_checked_time_ = time(NULL);
    new_version_ = plan.version;
    new_payload_size_ = plan.payload_size;
    SetupDownload();
    SetupCpuSharesManagement();
    SetStatusAndNotify(UPDATE_STATUS_UPDATE_AVAILABLE,
                       kUpdateNoticeUnspecified);
  } else if (type == DownloadAction::StaticType()) {
    SetStatusAndNotify(UPDATE_STATUS_FINALIZING, kUpdateNoticeUnspecified);
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
    SetStatusAndNotify(UPDATE_STATUS_DOWNLOADING, kUpdateNoticeUnspecified);
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
        const FilePath update_completed_marker_path(update_completed_marker_);
        if (!file_util::Delete(update_completed_marker_path, false))
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
  vector<string> cmd(1, "/usr/sbin/chromeos-setgoodkernel");
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

  if (!utils::IsNormalBootMode())
    flags |= kErrorCodeDevModeFlag;

  if (response_handler_action_.get() &&
      response_handler_action_->install_plan().is_resume)
    flags |= kErrorCodeResumedFlag;

  if (!utils::IsOfficialBuild())
    flags |= kErrorCodeTestImageFlag;

  if (omaha_request_params_->update_url() != kProductionOmahaUrl)
    flags |= kErrorCodeTestOmahaUrlFlag;

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
    *cancel_reason = kErrorCodeUpdateCanceledByChannelChange;
    return true;
  }

  return false;
}

void UpdateAttempter::SetStatusAndNotify(UpdateStatus status,
                                         UpdateNotice notice) {
  status_ = status;
  if (update_check_scheduler_) {
    update_check_scheduler_->SetUpdateStatus(status_, notice);
  }
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
       code == kErrorCodeError) ||
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
    case kErrorCodeOmahaUpdateIgnoredPerPolicy:
    case kErrorCodeOmahaUpdateDeferredPerPolicy:
    case kErrorCodeOmahaUpdateDeferredForBackoff:
      event_result = OmahaEvent::kResultUpdateDeferred;
      break;
    default:
      event_result = OmahaEvent::kResultError;
      break;
  }

  code = GetErrorCodeForAction(action, code);
  fake_update_success_ = code == kErrorCodePostinstallBootedFromFirmwareB;

  // Compute the final error code with all the bit flags to be sent to Omaha.
  code = static_cast<ErrorCode>(code | GetErrorCodeFlags());
  error_event_.reset(new OmahaEvent(OmahaEvent::kTypeUpdateComplete,
                                    event_result,
                                    code));
}

bool UpdateAttempter::ScheduleErrorEventAction() {
  if (error_event_.get() == NULL)
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
                                                    system_state_,
                                                    is_test_mode_),
                             false));
  actions_.push_back(shared_ptr<AbstractAction>(error_event_action));
  processor_->EnqueueAction(error_event_action.get());
  SetStatusAndNotify(UPDATE_STATUS_REPORTING_ERROR_EVENT,
                     kUpdateNoticeUnspecified);
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
                        NULL);
  g_source_attach(manage_shares_source_, NULL);
  SetCpuShares(utils::kCpuSharesLow);
}

void UpdateAttempter::CleanupCpuSharesManagement() {
  if (manage_shares_source_) {
    g_source_destroy(manage_shares_source_);
    manage_shares_source_ = NULL;
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
  manage_shares_source_ = NULL;
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
      dynamic_cast<MultiRangeHttpFetcher*>(download_action_->http_fetcher());
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
                               NULL,
                               new LibcurlHttpFetcher(GetProxyResolver(),
                                                      system_state_,
                                                      is_test_mode_),
                               true));
    actions_.push_back(shared_ptr<OmahaRequestAction>(ping_action));
    processor_->set_delegate(NULL);
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

  // Update the status which will schedule the next update check
  SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT,
                     kUpdateNoticeUnspecified);
}


bool UpdateAttempter::DecrementUpdateCheckCount() {
  int64 update_check_count_value;

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
  system_state_->payload_state()->UpdateEngineStarted();
  StartP2PAtStartup();
}

bool UpdateAttempter::StartP2PAtStartup() {
  if (system_state_ == NULL ||
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
  if (system_state_ == NULL)
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

}  // namespace chromeos_update_engine
