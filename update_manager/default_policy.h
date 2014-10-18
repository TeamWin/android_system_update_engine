// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_DEFAULT_POLICY_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_DEFAULT_POLICY_H_

#include <memory>
#include <string>

#include <base/time/time.h>

#include "update_engine/clock_interface.h"
#include "update_engine/update_manager/policy.h"

namespace chromeos_update_manager {

// Auxiliary state class for DefaultPolicy evaluations.
//
// IMPORTANT: The use of a state object in policies is generally forbidden, as
// it was a design decision to keep policy calls side-effect free. We make an
// exception here to ensure that DefaultPolicy indeed serves as a safe (and
// secure) fallback option. This practice should be avoided when imlpementing
// other policies.
class DefaultPolicyState {
 public:
  DefaultPolicyState() {}

  bool IsLastCheckAllowedTimeSet() const {
    return last_check_allowed_time_ != base::Time::Max();
  }

  // Sets/returns the point time on the monotonic time scale when the latest
  // check allowed was recorded.
  void set_last_check_allowed_time(base::Time timestamp) {
    last_check_allowed_time_ = timestamp;
  }
  base::Time last_check_allowed_time() const {
    return last_check_allowed_time_;
  }

 private:
  base::Time last_check_allowed_time_ = base::Time::Max();
};

// The DefaultPolicy is a safe Policy implementation that doesn't fail. The
// values returned by this policy are safe default in case of failure of the
// actual policy being used by the UpdateManager.
class DefaultPolicy : public Policy {
 public:
  explicit DefaultPolicy(chromeos_update_engine::ClockInterface* clock);
  DefaultPolicy() : DefaultPolicy(nullptr) {}
  virtual ~DefaultPolicy() {}

  // Policy overrides.
  EvalStatus UpdateCheckAllowed(
      EvaluationContext* ec, State* state, std::string* error,
      UpdateCheckParams* result) const override;

  EvalStatus UpdateCanStart(
      EvaluationContext* ec, State* state, std::string* error,
      UpdateDownloadParams* result,
      UpdateState update_state) const override;

  EvalStatus UpdateDownloadAllowed(
      EvaluationContext* ec, State* state, std::string* error,
      bool* result) const override;

 protected:
  // Policy override.
  std::string PolicyName() const override { return "DefaultPolicy"; }

 private:
  // A clock interface.
  chromeos_update_engine::ClockInterface* clock_;

  // An auxiliary state object.
  std::unique_ptr<DefaultPolicyState> aux_state_;

  DISALLOW_COPY_AND_ASSIGN(DefaultPolicy);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_DEFAULT_POLICY_H_
