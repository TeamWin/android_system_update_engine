// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_POLICY_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_POLICY_H_

#include <string>
#include <vector>

#include "update_engine/error_code.h"
#include "update_engine/update_manager/evaluation_context.h"
#include "update_engine/update_manager/state.h"

namespace chromeos_update_manager {

// The three different results of a policy request.
enum class EvalStatus {
  kFailed,
  kSucceeded,
  kAskMeAgainLater,
};

std::string ToString(EvalStatus status);

// Parameters of an update check. These parameters are determined by the
// UpdateCheckAllowed policy.
struct UpdateCheckParams {
  bool updates_enabled;  // Whether the auto-updates are enabled on this build.

  // Attributes pertaining to the case where update checks are allowed.
  //
  // A target version prefix, if imposed by policy; otherwise, an empty string.
  std::string target_version_prefix;
  // A target channel, if so imposed by policy; otherwise, an empty string.
  std::string target_channel;

  // Whether the allowed update is interactive (user-initiated) or periodic.
  bool is_interactive;
};

// Input arguments to UpdateCanStart.
//
// A snapshot of the state of the current update process.
struct UpdateState {
  // Information pertaining to the Omaha update response.
  //
  // Time when update was first offered by Omaha.
  base::Time first_seen;
  // Number of update checks returning the current update.
  int num_checks;

  // Information pertaining to the update download URL.
  //
  // An array of download URLs provided by Omaha.
  std::vector<std::string> download_urls;
  // Max number of failures allowed per download URL.
  int download_failures_max;
  // The index of the URL to use, as previously determined by the policy. This
  // number is significant iff |num_checks| is greater than 1.
  int download_url_idx;
  // The number of failures already associated with this URL.
  int download_url_num_failures;
  // An array of failure error codes that occurred since the latest reported
  // ones (included in the number above).
  std::vector<chromeos_update_engine::ErrorCode> download_url_error_codes;

  // Information pertaining to update scattering.
  //
  // Scattering wallclock-based wait period, as returned by the policy.
  base::TimeDelta scatter_wait_period;
  // Maximum wait period allowed for this update, as determined by Omaha.
  base::TimeDelta scatter_wait_period_max;
  // Scattering update check threshold, as returned by the policy.
  int scatter_check_threshold;
  // Minimum/maximum check threshold values.
  // TODO(garnold) These appear to not be related to the current update and so
  // should probably be obtained as variables via UpdaterProvider.
  int scatter_check_threshold_min;
  int scatter_check_threshold_max;
};

// Results regarding the downloading and applying of an update, as determined by
// UpdateCanStart.
//
// An enumerator for the reasons of not allowing an update to start.
enum class UpdateCannotStartReason {
  kUndefined,
  kCheckDue,
  kScattering,
  kCannotDownload,
};

struct UpdateDownloadParams {
  // Whether the update attempt is allowed to proceed.
  bool update_can_start;

  // Attributes pertaining to the case where update is allowed. The update
  // engine uses them to choose the means for downloading and applying an
  // update.
  bool p2p_allowed;
  // The index of the download URL to use, and the number of failures associated
  // with this URL. An index value of -1 indicates that no suitable URL is
  // available, but there may be other means for download (like P2P).
  int download_url_idx;
  int download_url_num_failures;

  // Attributes pertaining to the case where update is not allowed. Some are
  // needed for storing values to persistent storage, others for
  // logging/metrics.
  UpdateCannotStartReason cannot_start_reason;
  base::TimeDelta scatter_wait_period;  // Needs to be persisted.
  int scatter_check_threshold;  // Needs to be persisted.
};

// The Policy class is an interface to the ensemble of policy requests that the
// client can make. A derived class includes the policy implementations of
// these.
//
// When compile-time selection of the policy is required due to missing or extra
// parts in a given platform, a different Policy subclass can be used.
class Policy {
 public:
  virtual ~Policy() {}

  // Returns the name of a public policy request.
  // IMPORTANT: Be sure to add a conditional for each new public policy that is
  // being added to this class in the future.
  template<typename R, typename... Args>
  std::string PolicyRequestName(
      EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                          std::string*, R*,
                                          Args...) const) const {
    std::string class_name = PolicyName() + "::";

    if (reinterpret_cast<typeof(&Policy::UpdateCheckAllowed)>(
            policy_method) == &Policy::UpdateCheckAllowed)
      return class_name + "UpdateCheckAllowed";
    if (reinterpret_cast<typeof(&Policy::UpdateCanStart)>(
            policy_method) == &Policy::UpdateCanStart)
      return class_name + "UpdateCanStart";
    if (reinterpret_cast<typeof(&Policy::UpdateDownloadAllowed)>(
            policy_method) == &Policy::UpdateDownloadAllowed)
      return class_name + "UpdateDownloadAllowed";

    NOTREACHED();
    return class_name + "(unknown)";
  }


  // List of policy requests. A policy request takes an EvaluationContext as the
  // first argument, a State instance, a returned error message, a returned
  // value and optionally followed by one or more arbitrary constant arguments.
  //
  // When the implementation fails, the method returns EvalStatus::kFailed and
  // sets the |error| string.

  // UpdateCheckAllowed returns whether it is allowed to request an update check
  // to Omaha.
  virtual EvalStatus UpdateCheckAllowed(
      EvaluationContext* ec, State* state, std::string* error,
      UpdateCheckParams* result) const = 0;

  // Returns EvalStatus::kSucceeded if either an update can start being
  // processed, or the attempt needs to be aborted. In cases where the update
  // needs to wait for some condition to be satisfied, but none of the values
  // that need to be persisted has changed, returns
  // EvalStatus::kAskMeAgainLater. Arguments include an |interactive| flag that
  // tells whether the update is user initiated, and an |update_state| that
  // encapsulates data pertaining to the current ongoing update process.
  virtual EvalStatus UpdateCanStart(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      UpdateDownloadParams* result,
      const bool interactive,
      const UpdateState& update_state) const = 0;

  // Checks whether downloading of an update is allowed; currently, this checks
  // whether the network connection type is suitable for updating over.  May
  // consult the shill provider as well as the device policy (if available).
  // Returns |EvalStatus::kSucceeded|, setting |result| according to whether or
  // not the current connection can be used; on failure, returns
  // |EvalStatus::kFailed| and sets |error| accordingly.
  virtual EvalStatus UpdateDownloadAllowed(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      bool* result) const = 0;

 protected:
  Policy() {}

  // Returns the name of the actual policy class.
  virtual std::string PolicyName() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Policy);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_POLICY_H_
