// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_H_

#include <string>

#include "update_engine/policy_manager/evaluation_context.h"
#include "update_engine/policy_manager/state.h"

namespace chromeos_policy_manager {

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
};

// Input arguments to UpdateCanStart.
//
// A snapshot of the state of the current update process.
struct UpdateState {
  // Time when update was first offered by Omaha.
  base::Time first_seen;
  // Number of update checks returning the current update.
  int num_checks;
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
  kDisabledByPolicy,
  kScattering,
};

struct UpdateCanStartResult {
  // Whether the update attempt is allowed to proceed.
  bool update_can_start;
  // Attributes pertaining to the case where update is allowed. The update
  // engine uses them to choose the means for downloading and applying an
  // update.
  bool http_allowed;
  bool p2p_allowed;
  std::string target_channel;
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
  // encapsulates data pertaining to the currnet ongoing update process.
  virtual EvalStatus UpdateCanStart(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      UpdateCanStartResult* result,
      const bool interactive,
      const UpdateState& update_state) const = 0;

  // Checks whether updating is allowed over the current network connection
  // Consults the shill provider as well as the device policy (if available).
  // Returns |EvalStatus::kSucceeded|, setting |result| according to whether or
  // not the current connection can be used; on failure, returns
  // |EvalStatus::kFailed| and sets |error| accordingly.
  virtual EvalStatus UpdateCurrentConnectionAllowed(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      bool* result) const = 0;

 protected:
  Policy() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Policy);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_H_
