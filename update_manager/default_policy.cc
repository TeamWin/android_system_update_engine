// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/default_policy.h"

namespace {

// A fixed minimum interval between consecutive allowed update checks. This
// needs to be long enough to prevent busywork and/or DDoS attacks on Omaha, but
// at the same time short enough to allow the machine to update itself
// reasonably soon.
const int kCheckIntervalInSeconds = 15 * 60;

}  // namespace

namespace chromeos_update_manager {

DefaultPolicy::DefaultPolicy(chromeos_update_engine::ClockInterface* clock)
    : clock_(clock), aux_state_(new DefaultPolicyState()) {}

EvalStatus DefaultPolicy::UpdateCheckAllowed(
    EvaluationContext* ec, State* state, std::string* error,
    UpdateCheckParams* result) const {
  result->updates_enabled = true;
  result->target_channel.clear();
  result->target_version_prefix.clear();
  result->is_interactive = false;

  // Ensure that the minimum interval is set. If there's no clock, this defaults
  // to always allowing the update.
  if (!aux_state_->IsLastCheckAllowedTimeSet() ||
      ec->IsMonotonicTimeGreaterThan(
          aux_state_->last_check_allowed_time() +
          base::TimeDelta::FromSeconds(kCheckIntervalInSeconds))) {
    if (clock_)
      aux_state_->set_last_check_allowed_time(clock_->GetMonotonicTime());
    return EvalStatus::kSucceeded;
  }

  return EvalStatus::kAskMeAgainLater;
}

}  // namespace chromeos_update_manager
