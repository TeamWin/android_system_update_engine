// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_DEFAULT_POLICY_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_DEFAULT_POLICY_H_

#include <base/time/time.h>

#include "update_engine/policy_manager/policy.h"

namespace chromeos_policy_manager {

// The DefaultPolicy is a safe Policy implementation that doesn't fail. The
// values returned by this policy are safe default in case of failure of the
// actual policy being used by the PolicyManager.
class DefaultPolicy : public Policy {
 public:
  DefaultPolicy() {}
  virtual ~DefaultPolicy() {}

  // Policy overrides.
  virtual EvalStatus UpdateCheckAllowed(
      EvaluationContext* ec, State* state, std::string* error,
      UpdateCheckParams* result) const override {
    result->updates_enabled = true;
    return EvalStatus::kSucceeded;
  }

  virtual EvalStatus UpdateCanStart(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      UpdateCanStartResult* result,
      const bool interactive,
      const UpdateState& update_state) const override {
    result->update_can_start = true;
    result->http_allowed = false;
    result->p2p_allowed = false;
    result->target_channel.clear();
    result->cannot_start_reason = UpdateCannotStartReason::kUndefined;
    result->scatter_wait_period = base::TimeDelta();
    result->scatter_check_threshold = 0;
    return EvalStatus::kSucceeded;
  }

  virtual EvalStatus UpdateCurrentConnectionAllowed(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      bool* result) const override {
    *result = true;
    return EvalStatus::kSucceeded;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPolicy);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_DEFAULT_POLICY_H_
