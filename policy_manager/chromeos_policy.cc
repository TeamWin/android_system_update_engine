// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/chromeos_policy.h"
#include "update_engine/policy_manager/policy_utils.h"

#include <string>

using base::Time;
using base::TimeDelta;
using std::string;

namespace chromeos_policy_manager {

EvalStatus ChromeOSPolicy::UpdateCheckAllowed(
    EvaluationContext* ec, State* state, string* error,
    UpdateCheckParams* result) const {
  Time next_update_check;
  if (NextUpdateCheckTime(ec, state, error, &next_update_check) !=
      EvalStatus::kSucceeded) {
    return EvalStatus::kFailed;
  }

  if (!ec->IsTimeGreaterThan(next_update_check))
    return EvalStatus::kAskMeAgainLater;

  // It is time to check for an update.
  result->updates_enabled = true;
  return EvalStatus::kSucceeded;
}

EvalStatus ChromeOSPolicy::UpdateDownloadAndApplyAllowed(EvaluationContext* ec,
                                                         State* state,
                                                         string* error,
                                                         bool* result) const {
  // TODO(garnold): Write this policy implementation with the actual policy.
  *result = true;
  return EvalStatus::kSucceeded;
}

EvalStatus ChromeOSPolicy::NextUpdateCheckTime(EvaluationContext* ec,
                                               State* state, string* error,
                                               Time* next_update_check) const {
  // Don't check for updates too often. We limit the update checks to once every
  // some interval. The interval is kTimeoutInitialInterval the first time and
  // kTimeoutPeriodicInterval for the subsequent update checks. If the update
  // check fails, we increase the interval between the update checks
  // exponentially until kTimeoutMaxBackoffInterval. Finally, to avoid having
  // many chromebooks running update checks at the exact same time, we add some
  // fuzz to the interval.
  const Time* updater_started_time =
      ec->GetValue(state->updater_provider()->var_updater_started_time());
  POLICY_CHECK_VALUE_AND_FAIL(updater_started_time, error);

  const base::Time* last_checked_time =
      ec->GetValue(state->updater_provider()->var_last_checked_time());

  const uint64_t* seed = ec->GetValue(state->random_provider()->var_seed());
  POLICY_CHECK_VALUE_AND_FAIL(seed, error);

  PRNG prng(*seed);

  if (!last_checked_time || *last_checked_time < *updater_started_time) {
    // First attempt.
    *next_update_check = *updater_started_time + FuzzedInterval(
        &prng, kTimeoutInitialInterval, kTimeoutRegularFuzz);
    return EvalStatus::kSucceeded;
  }
  // Check for previous failed attempts to implement the exponential backoff.
  const unsigned int* consecutive_failed_update_checks = ec->GetValue(
      state->updater_provider()->var_consecutive_failed_update_checks());
  POLICY_CHECK_VALUE_AND_FAIL(consecutive_failed_update_checks, error);

  int interval = kTimeoutInitialInterval;
  for (unsigned int i = 0; i < *consecutive_failed_update_checks; ++i) {
    interval *= 2;
    if (interval > kTimeoutMaxBackoffInterval) {
      interval = kTimeoutMaxBackoffInterval;
      break;
    }
  }

  *next_update_check = *last_checked_time + FuzzedInterval(
      &prng, interval, kTimeoutRegularFuzz);
  return EvalStatus::kSucceeded;
}

TimeDelta ChromeOSPolicy::FuzzedInterval(PRNG* prng, int interval, int fuzz) {
  int half_fuzz = fuzz / 2;
  int lower_bound = interval - half_fuzz;
  int upper_bound = interval + half_fuzz + 1;

  // This guarantees the output interval is non negative.
  if (lower_bound < 0)
    lower_bound = 0;

  int fuzzed_interval = lower_bound;
  if (upper_bound - lower_bound > 0)
    fuzzed_interval = lower_bound + prng->rand() % (upper_bound - lower_bound);
  return TimeDelta::FromSeconds(fuzzed_interval);
}

}  // namespace chromeos_policy_manager
