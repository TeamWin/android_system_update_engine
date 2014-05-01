// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/policy_manager/default_policy.h"
#include "update_engine/policy_manager/fake_state.h"
#include "update_engine/policy_manager/mock_policy.h"
#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/policy_manager/policy_manager.h"
#include "update_engine/test_utils.h"

using base::Bind;
using base::Callback;
using base::Time;
using base::TimeDelta;
using chromeos_update_engine::FakeClock;
using std::pair;
using std::string;
using std::vector;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace {

// Generates a fixed timestamp for use in faking the current time.
Time FixedTime() {
  Time::Exploded now_exp;
  now_exp.year = 2014;
  now_exp.month = 3;
  now_exp.day_of_week = 2;
  now_exp.day_of_month = 18;
  now_exp.hour = 8;
  now_exp.minute = 5;
  now_exp.second = 33;
  now_exp.millisecond = 675;
  return Time::FromLocalExploded(now_exp);
}

}  // namespace

namespace chromeos_policy_manager {

class PmPolicyManagerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    fake_state_ = new FakeState();
    pmut_.reset(new PolicyManager(&fake_clock_, fake_state_));
  }

  FakeState* fake_state_;  // Owned by the pmut_.
  FakeClock fake_clock_;
  scoped_ptr<PolicyManager> pmut_;
};

// The FailingPolicy implements a single method and make it always fail. This
// class extends the DefaultPolicy class to allow extensions of the Policy
// class without extending nor changing this test.
class FailingPolicy : public DefaultPolicy {
  virtual EvalStatus UpdateCheckAllowed(EvaluationContext* ec, State* state,
                                        string* error,
                                        UpdateCheckParams* result) const {
    *error = "FailingPolicy failed.";
    return EvalStatus::kFailed;
  }
};

// The LazyPolicy always returns EvalStatus::kAskMeAgainLater.
class LazyPolicy : public DefaultPolicy {
  virtual EvalStatus UpdateCheckAllowed(EvaluationContext* ec, State* state,
                                        string* error,
                                        UpdateCheckParams* result) const {
    return EvalStatus::kAskMeAgainLater;
  }
};

// AccumulateCallsCallback() adds to the passed |acc| accumulator vector pairs
// of EvalStatus and T instances. This allows to create a callback that keeps
// track of when it is called and the arguments passed to it, to be used with
// the PolicyManager::AsyncPolicyRequest().
template<typename T>
static void AccumulateCallsCallback(vector<pair<EvalStatus, T>>* acc,
                                    EvalStatus status, const T& result) {
  acc->push_back(std::make_pair(status, result));
}

// Tests that policy requests are completed successfully. It is important that
// this tests cover all policy requests as defined in Policy.
TEST_F(PmPolicyManagerTest, PolicyRequestCallUpdateCheckAllowed) {
  UpdateCheckParams result;
  EXPECT_EQ(EvalStatus::kSucceeded, pmut_->PolicyRequest(
      &Policy::UpdateCheckAllowed, &result));
}

TEST_F(PmPolicyManagerTest, PolicyRequestCallUpdateCanStart) {
  const UpdateState update_state = {
    FixedTime(), 1, TimeDelta::FromSeconds(15), TimeDelta::FromSeconds(60),
    4, 2, 8
  };
  UpdateCanStartResult result;
  EXPECT_EQ(EvalStatus::kSucceeded,
            pmut_->PolicyRequest(&Policy::UpdateCanStart, &result, true,
                                 update_state));
}

TEST_F(PmPolicyManagerTest, PolicyRequestCallsDefaultOnError) {
  pmut_->set_policy(new FailingPolicy());

  // Tests that the DefaultPolicy instance is called when the method fails,
  // which will set this as true.
  UpdateCheckParams result;
  result.updates_enabled = false;
  EvalStatus status = pmut_->PolicyRequest(
      &Policy::UpdateCheckAllowed, &result);
  EXPECT_EQ(EvalStatus::kSucceeded, status);
  EXPECT_TRUE(result.updates_enabled);
}

TEST_F(PmPolicyManagerTest, PolicyRequestDoesntBlock) {
  UpdateCheckParams result;
  pmut_->set_policy(new LazyPolicy());

  EvalStatus status = pmut_->PolicyRequest(
      &Policy::UpdateCheckAllowed, &result);
  EXPECT_EQ(EvalStatus::kAskMeAgainLater, status);
}

TEST_F(PmPolicyManagerTest, AsyncPolicyRequestDelaysEvaluation) {
  // To avoid differences in code execution order between an AsyncPolicyRequest
  // call on a policy that returns AskMeAgainLater the first time and one that
  // succeeds the first time, we ensure that the passed callback is called from
  // the main loop in both cases even when we could evaluate it right now.
  pmut_->set_policy(new FailingPolicy());

  vector<pair<EvalStatus, UpdateCheckParams>> calls;
  Callback<void(EvalStatus, const UpdateCheckParams& result)> callback =
      Bind(AccumulateCallsCallback<UpdateCheckParams>, &calls);

  pmut_->AsyncPolicyRequest(callback, &Policy::UpdateCheckAllowed);
  // The callback should wait until we run the main loop for it to be executed.
  EXPECT_EQ(0, calls.size());
  chromeos_update_engine::RunGMainLoopMaxIterations(100);
  EXPECT_EQ(1, calls.size());
}

}  // namespace chromeos_policy_manager
