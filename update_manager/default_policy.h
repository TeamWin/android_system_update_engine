// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_DEFAULT_POLICY_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_DEFAULT_POLICY_H_

#include <string>

#include <base/time/time.h>

#include "update_engine/update_manager/policy.h"

namespace chromeos_update_manager {

// The DefaultPolicy is a safe Policy implementation that doesn't fail. The
// values returned by this policy are safe default in case of failure of the
// actual policy being used by the UpdateManager.
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
    result->p2p_allowed = false;
    result->target_channel.clear();
    result->download_url_idx = 0;
    result->download_url_num_failures = 0;
    result->cannot_start_reason = UpdateCannotStartReason::kUndefined;
    result->scatter_wait_period = base::TimeDelta();
    result->scatter_check_threshold = 0;
    return EvalStatus::kSucceeded;
  }

  virtual EvalStatus UpdateDownloadAllowed(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      bool* result) const override {
    *result = true;
    return EvalStatus::kSucceeded;
  }

 protected:
  // Policy override.
  virtual std::string PolicyName() const override {
    return "DefaultPolicy";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPolicy);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_DEFAULT_POLICY_H_
