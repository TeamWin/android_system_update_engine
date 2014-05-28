// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_CHROMEOS_POLICY_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_CHROMEOS_POLICY_H_

#include <base/time/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/update_manager/policy.h"
#include "update_engine/update_manager/prng.h"

namespace chromeos_update_manager {

// Parameters for update scattering, as determined by UpdateNotScattering.
struct UpdateScatteringResult {
  bool is_scattering;
  base::TimeDelta wait_period;
  int check_threshold;
};

// ChromeOSPolicy implements the policy-related logic used in ChromeOS.
class ChromeOSPolicy : public Policy {
 public:
  ChromeOSPolicy() {}
  virtual ~ChromeOSPolicy() {}

  // Policy overrides.
  virtual EvalStatus UpdateCheckAllowed(
      EvaluationContext* ec, State* state, std::string* error,
      UpdateCheckParams* result) const override;

  virtual EvalStatus UpdateCanStart(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      UpdateCanStartResult* result,
      const bool interactive,
      const UpdateState& update_state) const override;

  virtual EvalStatus UpdateCurrentConnectionAllowed(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      bool* result) const override;

 private:
  friend class UmChromeOSPolicyTest;
  FRIEND_TEST(UmChromeOSPolicyTest,
              FirstCheckIsAtMostInitialIntervalAfterStart);
  FRIEND_TEST(UmChromeOSPolicyTest, ExponentialBackoffIsCapped);
  FRIEND_TEST(UmChromeOSPolicyTest, UpdateCheckAllowedWaitsForTheTimeout);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartNotAllowedScatteringNewWaitPeriodApplies);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartNotAllowedScatteringPrevWaitPeriodStillApplies);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartNotAllowedScatteringNewCountThresholdApplies);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartNotAllowedScatteringPrevCountThresholdStillApplies);
  FRIEND_TEST(UmChromeOSPolicyTest, UpdateCanStartAllowedScatteringSatisfied);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartAllowedInteractivePreventsScattering);

  // Auxiliary constant (zero by default).
  const base::TimeDelta kZeroInterval;

  // Default update check timeout interval/fuzz values used to compute the
  // NextUpdateCheckTime(), in seconds. Actual fuzz is within +/- half of the
  // indicated value.
  static const int kTimeoutInitialInterval    =  7 * 60;
  static const int kTimeoutPeriodicInterval   = 45 * 60;
  static const int kTimeoutQuickInterval      =  1 * 60;
  static const int kTimeoutMaxBackoffInterval =  4 * 60 * 60;
  static const int kTimeoutRegularFuzz        = 10 * 60;

  // A private policy implementation returning the wallclock timestamp when
  // the next update check should happen.
  EvalStatus NextUpdateCheckTime(EvaluationContext* ec, State* state,
                                 std::string* error,
                                 base::Time* next_update_check) const;

  // Returns a TimeDelta based on the provided |interval| seconds +/- half
  // |fuzz| seconds. The return value is guaranteed to be a non-negative
  // TimeDelta.
  static base::TimeDelta FuzzedInterval(PRNG* prng, int interval, int fuzz);

  // A private policy for checking whether scattering is due. Writes in |result|
  // the decision as to whether or not to scatter; a wallclock-based scatter
  // wait period, which ranges from zero (do not wait) and no greater than the
  // current scatter factor provided by the device policy (if available) or the
  // maximum wait period determined by Omaha; and an update check-based
  // threshold between zero (no threshold) and the maximum number determined by
  // the update engine. Within |update_state|, |wait_period| should contain the
  // last scattering period returned by this function, or zero if no wait period
  // is known; |check_threshold| is the last update check threshold, or zero if
  // no such threshold is known. If not scattering, or if any of the scattering
  // values has changed, returns |EvalStatus::kSucceeded|; otherwise,
  // |EvalStatus::kAskMeAgainLater|.
  EvalStatus UpdateScattering(EvaluationContext* ec, State* state,
                              std::string* error,
                              UpdateScatteringResult* result,
                              const UpdateState& update_state) const;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSPolicy);
};

}  // namespace chromeos_update_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_CHROMEOS_POLICY_H_
