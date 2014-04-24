// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_CHROMEOS_POLICY_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_CHROMEOS_POLICY_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/policy_manager/policy.h"
#include "update_engine/policy_manager/prng.h"

namespace chromeos_policy_manager {

// ChromeOSPolicy implements the policy-related logic used in ChromeOS.
class ChromeOSPolicy : public Policy {
 public:
  ChromeOSPolicy() {}
  virtual ~ChromeOSPolicy() {}

  // Policy overrides.
  virtual EvalStatus UpdateCheckAllowed(
      EvaluationContext* ec, State* state, std::string* error,
      UpdateCheckParams* result) const override;

  virtual EvalStatus UpdateDownloadAndApplyAllowed(EvaluationContext* ec,
                                                   State* state,
                                                   std::string* error,
                                                   bool* result) const override;

 private:
  FRIEND_TEST(PmChromeOSPolicyTest,
              FirstCheckIsAtMostInitialIntervalAfterStart);
  FRIEND_TEST(PmChromeOSPolicyTest, ExponentialBackoffIsCapped);
  FRIEND_TEST(PmChromeOSPolicyTest, UpdateCheckAllowedWaitsForTheTimeout);

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

  DISALLOW_COPY_AND_ASSIGN(ChromeOSPolicy);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_CHROMEOS_POLICY_H_
