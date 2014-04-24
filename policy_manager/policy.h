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

  // Returns whether an update can be downloaded/applied.
  virtual EvalStatus UpdateDownloadAndApplyAllowed(EvaluationContext* ec,
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
