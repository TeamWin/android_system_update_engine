// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <base/time.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>

#include "update_engine/policy_manager/default_policy.h"
#include "update_engine/policy_manager/mock_policy.h"
#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/policy_manager/policy_manager.h"

using base::TimeDelta;
using std::string;

using testing::_;
using testing::Return;
using testing::StrictMock;

namespace chromeos_policy_manager {

class PmPolicyManagerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    EXPECT_TRUE(pmut_.Init());
  }

  PolicyManager pmut_; // PolicyManager undert test.
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

// The LazyPolicy always returns
class LazyPolicy : public DefaultPolicy {
  virtual EvalStatus UpdateCheckAllowed(EvaluationContext* ec, State* state,
                                        string* error,
                                        bool* result) const {
    return EvalStatus::kAskMeAgainLater;
  }
};

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

}  // namespace chromeos_policy_manager
