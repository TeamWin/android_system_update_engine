// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/chromeos_policy.h"

#include <string>

#include <base/time/time.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/policy_manager/evaluation_context.h"
#include "update_engine/policy_manager/fake_state.h"
#include "update_engine/policy_manager/pmtest_utils.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::FakeClock;
using std::string;

namespace chromeos_policy_manager {

class PmChromeOSPolicyTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SetUpDefaultClock();
    eval_ctx_ = new EvaluationContext(&fake_clock_);
  }

  // Sets the clock to fixed values.
  void SetUpDefaultClock() {
    fake_clock_.SetMonotonicTime(Time::FromInternalValue(12345678L));
    fake_clock_.SetWallclockTime(Time::FromInternalValue(12345678901234L));
  }

  void SetUpDefaultState() {
    fake_state_.updater_provider()->var_updater_started_time()->reset(
        new Time(fake_clock_.GetWallclockTime()));
    fake_state_.updater_provider()->var_last_checked_time()->reset(
        new Time(fake_clock_.GetWallclockTime()));
    fake_state_.updater_provider()->var_consecutive_failed_update_checks()->
        reset(new unsigned int(0));

    fake_state_.random_provider()->var_seed()->reset(
        new uint64_t(4));  // chosen by fair dice roll.
                           // guaranteed to be random.
  }

  // Runs the passed |policy_method| policy and expects it to return the
  // |expected| return value.
  template<typename T, typename R, typename... Args>
  void ExpectPolicyStatus(
      EvalStatus expected,
      T policy_method,
      R* result, Args... args) {
    string error = "<None>";
    eval_ctx_->ResetEvaluation();
    EXPECT_EQ(expected,
              (policy_.*policy_method)(eval_ctx_, &fake_state_, &error, result))
        << "Returned error: " << error
        << "\nEvaluation context: " << eval_ctx_->DumpContext();
  }

  FakeClock fake_clock_;
  FakeState fake_state_;
  scoped_refptr<EvaluationContext> eval_ctx_;
  ChromeOSPolicy policy_;  // ChromeOSPolicy under test.
};

TEST_F(PmChromeOSPolicyTest, FirstCheckIsAtMostInitialIntervalAfterStart) {
  Time next_update_check;

  SetUpDefaultState();
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &ChromeOSPolicy::NextUpdateCheckTime, &next_update_check);

  EXPECT_LE(fake_clock_.GetWallclockTime(), next_update_check);
  EXPECT_GE(fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(
      ChromeOSPolicy::kTimeoutInitialInterval +
      ChromeOSPolicy::kTimeoutRegularFuzz), next_update_check);
}

TEST_F(PmChromeOSPolicyTest, ExponentialBackoffIsCapped) {
  Time next_update_check;

  SetUpDefaultState();
  fake_state_.updater_provider()->var_consecutive_failed_update_checks()->
      reset(new unsigned int(100));
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &ChromeOSPolicy::NextUpdateCheckTime, &next_update_check);

  EXPECT_LE(fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(
      ChromeOSPolicy::kTimeoutMaxBackoffInterval -
      ChromeOSPolicy::kTimeoutRegularFuzz - 1), next_update_check);
  EXPECT_GE(fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(
      ChromeOSPolicy::kTimeoutMaxBackoffInterval +
      ChromeOSPolicy::kTimeoutRegularFuzz), next_update_check);
}

TEST_F(PmChromeOSPolicyTest, UpdateCheckAllowedWaitsForTheTimeout) {
  // We get the next update_check timestamp from the policy's private method
  // and then we check the public method respects that value on the normal
  // case.
  Time next_update_check;
  Time last_checked_time =
      fake_clock_.GetWallclockTime() + TimeDelta::FromMinutes(1234);

  SetUpDefaultClock();
  SetUpDefaultState();
  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &ChromeOSPolicy::NextUpdateCheckTime, &next_update_check);

  UpdateCheckParams result;

  // Check that the policy blocks until the next_update_check is reached.
  SetUpDefaultClock();
  SetUpDefaultState();
  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  fake_clock_.SetWallclockTime(next_update_check - TimeDelta::FromSeconds(1));
  ExpectPolicyStatus(EvalStatus::kAskMeAgainLater,
                     &Policy::UpdateCheckAllowed, &result);

  SetUpDefaultClock();
  SetUpDefaultState();
  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  fake_clock_.SetWallclockTime(next_update_check + TimeDelta::FromSeconds(1));
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateCheckAllowed, &result);
}

}  // namespace chromeos_policy_manager
