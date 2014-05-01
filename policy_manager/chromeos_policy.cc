// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/chromeos_policy.h"

#include <algorithm>
#include <string>

#include <base/logging.h>
#include <base/time/time.h>

#include "update_engine/policy_manager/device_policy_provider.h"
#include "update_engine/policy_manager/policy_utils.h"

using base::Time;
using base::TimeDelta;
using std::min;
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

EvalStatus ChromeOSPolicy::UpdateCanStart(
    EvaluationContext* ec,
    State* state,
    string* error,
    UpdateCanStartResult* result,
    const bool interactive,
    const UpdateState& update_state) const {
  // Set the default return values.
  result->update_can_start = true;
  result->http_allowed = true;
  result->p2p_allowed = false;
  result->target_channel.clear();
  result->cannot_start_reason = UpdateCannotStartReason::kUndefined;
  result->scatter_wait_period = kZeroInterval;
  result->scatter_check_threshold = 0;

  // Make sure that we're not due for an update check.
  UpdateCheckParams check_result;
  EvalStatus check_status = UpdateCheckAllowed(ec, state, error, &check_result);
  if (check_status == EvalStatus::kFailed)
    return EvalStatus::kFailed;
  if (check_status == EvalStatus::kSucceeded &&
      check_result.updates_enabled == true) {
    result->update_can_start = false;
    result->cannot_start_reason = UpdateCannotStartReason::kCheckDue;
    return EvalStatus::kSucceeded;
  }

  DevicePolicyProvider* const dp_provider = state->device_policy_provider();

  const bool* device_policy_is_loaded_p = ec->GetValue(
      dp_provider->var_device_policy_is_loaded());
  if (device_policy_is_loaded_p && *device_policy_is_loaded_p) {
    // Ensure that update is enabled.
    const bool* update_disabled_p = ec->GetValue(
        dp_provider->var_update_disabled());
    if (update_disabled_p && *update_disabled_p) {
      result->update_can_start = false;
      result->cannot_start_reason = UpdateCannotStartReason::kDisabledByPolicy;
      return EvalStatus::kAskMeAgainLater;
    }

    // Check whether scattering applies to this update attempt.
    // TODO(garnold) We should not be scattering during OOBE. We'll need to read
    // the OOBE status (via SystemProvider) and only scatter if not enacted.
    // TODO(garnold) Current code further suppresses scattering if a "deadline"
    // attribute is found in the Omaha response. However, it appears that the
    // presence of this attribute is merely indicative of an OOBE update, which
    // we should support anyway (see above).
    if (!interactive) {
      UpdateScatteringResult scatter_result;
      EvalStatus scattering_status = UpdateScattering(
          ec, state, error, &scatter_result, update_state);
      if (scattering_status != EvalStatus::kSucceeded ||
          scatter_result.is_scattering) {
        if (scattering_status != EvalStatus::kFailed) {
          result->update_can_start = false;
          result->cannot_start_reason = UpdateCannotStartReason::kScattering;
          result->scatter_wait_period = scatter_result.wait_period;
          result->scatter_check_threshold = scatter_result.check_threshold;
        }
        return scattering_status;
      }
    }

    // Determine whether HTTP downloads are forbidden by policy. This only
    // applies to official system builds; otherwise, HTTP is always enabled.
    const bool* is_official_build_p = ec->GetValue(
        state->system_provider()->var_is_official_build());
    if (is_official_build_p && *is_official_build_p) {
      const bool* policy_http_downloads_enabled_p = ec->GetValue(
          dp_provider->var_http_downloads_enabled());
      result->http_allowed =
          !policy_http_downloads_enabled_p || *policy_http_downloads_enabled_p;
    }

    // Determine whether use of P2P is allowed by policy.
    const bool* policy_au_p2p_enabled_p = ec->GetValue(
        dp_provider->var_au_p2p_enabled());
    result->p2p_allowed = policy_au_p2p_enabled_p && *policy_au_p2p_enabled_p;

    // Determine whether a target channel is dictated by policy.
    const bool* release_channel_delegated_p = ec->GetValue(
        dp_provider->var_release_channel_delegated());
    if (release_channel_delegated_p && !(*release_channel_delegated_p)) {
      const string* release_channel_p = ec->GetValue(
          dp_provider->var_release_channel());
      if (release_channel_p)
        result->target_channel = *release_channel_p;
    }
  }

  // Enable P2P, if so mandated by the updater configuration.
  if (!result->p2p_allowed) {
    const bool* updater_p2p_enabled_p = ec->GetValue(
        state->updater_provider()->var_p2p_enabled());
    result->p2p_allowed = updater_p2p_enabled_p && *updater_p2p_enabled_p;
  }

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
  DCHECK_GE(interval, 0);
  DCHECK_GE(fuzz, 0);
  int half_fuzz = fuzz / 2;
  // This guarantees the output interval is non negative.
  int interval_min = std::max(interval - half_fuzz, 0);
  int interval_max = interval + half_fuzz;
  return TimeDelta::FromSeconds(prng->RandMinMax(interval_min, interval_max));
}

EvalStatus ChromeOSPolicy::UpdateScattering(
    EvaluationContext* ec,
    State* state,
    string* error,
    UpdateScatteringResult* result,
    const UpdateState& update_state) const {
  // Preconditions. These stem from the postconditions and usage contract.
  DCHECK(update_state.scatter_wait_period >= kZeroInterval);
  DCHECK_GE(update_state.scatter_check_threshold, 0);

  // Set default result values.
  result->is_scattering = false;
  result->wait_period = kZeroInterval;
  result->check_threshold = 0;

  DevicePolicyProvider* const dp_provider = state->device_policy_provider();

  // Ensure that a device policy is loaded.
  const bool* device_policy_is_loaded_p = ec->GetValue(
      dp_provider->var_device_policy_is_loaded());
  if (!(device_policy_is_loaded_p && *device_policy_is_loaded_p))
    return EvalStatus::kSucceeded;

  // Is scattering enabled by policy?
  const TimeDelta* scatter_factor_p = ec->GetValue(
      dp_provider->var_scatter_factor());
  if (!scatter_factor_p || *scatter_factor_p == kZeroInterval)
    return EvalStatus::kSucceeded;

  // Obtain a pseudo-random number generator.
  const uint64_t* seed = ec->GetValue(state->random_provider()->var_seed());
  POLICY_CHECK_VALUE_AND_FAIL(seed, error);
  PRNG prng(*seed);

  // Step 1: Maintain the scattering wait period.
  //
  // If no wait period was previously determined, or it no longer fits in the
  // scatter factor, then generate a new one. Otherwise, keep the one we have.
  // TODO(garnold) Current code (UpdateAttempter::GenerateNewWaitingPeriod())
  // always generates a non-zero value, which seems to imply that *some*
  // scattering always happens. Yet to validate whether this is intentional.
  TimeDelta wait_period = update_state.scatter_wait_period;
  if (wait_period == kZeroInterval || wait_period > *scatter_factor_p) {
    wait_period = TimeDelta::FromSeconds(
        prng.RandMinMax(1, scatter_factor_p->InSeconds()));
  }

  // If we surpass the wait period or the max scatter period associated with
  // the update, then no wait is needed.
  Time wait_expires = (update_state.first_seen +
                       min(wait_period, update_state.scatter_wait_period_max));
  if (ec->IsTimeGreaterThan(wait_expires))
    wait_period = kZeroInterval;

  // Step 2: Maintain the update check threshold count.
  //
  // If an update check threshold is not specified then generate a new
  // one.
  int check_threshold = update_state.scatter_check_threshold;
  if (check_threshold == 0) {
    check_threshold = prng.RandMinMax(
        update_state.scatter_check_threshold_min,
        update_state.scatter_check_threshold_max);
  }

  // If the update check threshold is not within allowed range then nullify it.
  // TODO(garnold) This is compliant with current logic found in
  // OmahaRequestAction::IsUpdateCheckCountBasedWaitingSatisfied(). We may want
  // to change it so that it behaves similarly to the wait period case, namely
  // if the current value exceeds the maximum, we set a new one within range.
  if (check_threshold > update_state.scatter_check_threshold_max)
    check_threshold = 0;

  // If the update check threshold is non-zero and satisfied, then nullify it.
  if (check_threshold > 0 && update_state.num_checks >= check_threshold)
    check_threshold = 0;

  bool is_scattering = (wait_period != kZeroInterval || check_threshold);
  EvalStatus ret = EvalStatus::kSucceeded;
  if (is_scattering && wait_period == update_state.scatter_wait_period &&
      check_threshold == update_state.scatter_check_threshold)
    ret = EvalStatus::kAskMeAgainLater;
  result->is_scattering = is_scattering;
  result->wait_period = wait_period;
  result->check_threshold = check_threshold;
  return ret;
}

}  // namespace chromeos_policy_manager
