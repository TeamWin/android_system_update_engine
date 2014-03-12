// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "update_engine/policy_manager/default_policy.h"
#include "update_engine/policy_manager/fake_state.h"
#include "update_engine/policy_manager/mock_policy.h"
#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/policy_manager/policy_manager.h"
#include "update_engine/test_utils.h"

using base::Bind;
using base::Callback;
using std::pair;
using std::string;
using std::vector;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace chromeos_policy_manager {

class PmPolicyManagerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    FakeState* fake_state = new FakeState();
    ASSERT_TRUE(fake_state->Init());
    EXPECT_TRUE(pmut_.Init(fake_state));
  }

  PolicyManager pmut_;
};

// The FailingPolicy implements a single method and make it always fail. This
// class extends the DefaultPolicy class to allow extensions of the Policy
// class without extending nor changing this test.
class FailingPolicy : public DefaultPolicy {
  virtual EvalStatus UpdateCheckAllowed(EvaluationContext* ec, State* state,
                                        string* error,
                                        bool* result) const {
    *error = "FailingPolicy failed.";
    return EvalStatus::kFailed;
  }
};

// The LazyPolicy always returns EvalStatus::kAskMeAgainLater.
class LazyPolicy : public DefaultPolicy {
  virtual EvalStatus UpdateCheckAllowed(EvaluationContext* ec, State* state,
                                        string* error,
                                        bool* result) const {
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

TEST_F(PmPolicyManagerTest, PolicyRequestCall) {
  bool result;
  EvalStatus status = pmut_.PolicyRequest(&Policy::UpdateCheckAllowed, &result);
  EXPECT_EQ(status, EvalStatus::kSucceeded);
}

TEST_F(PmPolicyManagerTest, PolicyRequestCallsPolicy) {
  StrictMock<MockPolicy>* policy = new StrictMock<MockPolicy>();
  pmut_.policy_.reset(policy);
  bool result;

  // Tests that the method is called on the policy_ instance.
  EXPECT_CALL(*policy, UpdateCheckAllowed(_, _, _, _))
      .WillOnce(Return(EvalStatus::kSucceeded));
  EvalStatus status = pmut_.PolicyRequest(&Policy::UpdateCheckAllowed, &result);
  EXPECT_EQ(status, EvalStatus::kSucceeded);
}

TEST_F(PmPolicyManagerTest, PolicyRequestCallsDefaultOnError) {
  pmut_.policy_.reset(new FailingPolicy());

  // Tests that the DefaultPolicy instance is called when the method fails,
  // which will set this as true.
  bool result = false;
  EvalStatus status = pmut_.PolicyRequest(&Policy::UpdateCheckAllowed, &result);
  EXPECT_EQ(status, EvalStatus::kSucceeded);
  EXPECT_TRUE(result);
}

TEST_F(PmPolicyManagerTest, PolicyRequestDoesntBlock) {
  pmut_.policy_.reset(new LazyPolicy());
  bool result;

  EvalStatus status = pmut_.PolicyRequest(&Policy::UpdateCheckAllowed, &result);
  EXPECT_EQ(status, EvalStatus::kAskMeAgainLater);
}

TEST_F(PmPolicyManagerTest, AsyncPolicyRequestDelaysEvaluation) {
  // To avoid differences in code execution order between an AsyncPolicyRequest
  // call on a policy that returns AskMeAgainLater the first time and one that
  // succeeds the first time, we ensure that the passed callback is called from
  // the main loop in both cases even when we could evaluate it right now.
  pmut_.policy_.reset(new FailingPolicy());

  vector<pair<EvalStatus, bool>> calls;
  Callback<void(EvalStatus, const bool& result)> callback =
      Bind(AccumulateCallsCallback<bool>, &calls);

  pmut_.AsyncPolicyRequest(callback, &Policy::UpdateCheckAllowed);
  // The callback should wait until we run the main loop for it to be executed.
  EXPECT_EQ(0, calls.size());
  chromeos_update_engine::RunGMainLoopMaxIterations(100);
  EXPECT_EQ(1, calls.size());
}

}  // namespace chromeos_policy_manager
