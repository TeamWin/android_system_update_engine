// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/bind.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/policy_manager/evaluation_context.h"
#include "update_engine/policy_manager/fake_variable.h"
#include "update_engine/policy_manager/generic_variables.h"
#include "update_engine/policy_manager/mock_variable.h"
#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/test_utils.h"

using base::Bind;
using base::Time;
using base::TimeDelta;
using chromeos_update_engine::FakeClock;
using chromeos_update_engine::RunGMainLoopMaxIterations;
using chromeos_update_engine::RunGMainLoopUntil;
using std::string;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace {

void DoNothing() {}

// Sets the value of the passed pointer to true.
void SetTrue(bool* value) {
  *value = true;
}

bool GetBoolean(bool* value) {
  return *value;
}

}  // namespace

namespace chromeos_policy_manager {

class PmEvaluationContextTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // Set the clock to a fixed values.
    fake_clock_.SetMonotonicTime(Time::FromInternalValue(12345678L));
    fake_clock_.SetWallclockTime(Time::FromInternalValue(12345678901234L));
    eval_ctx_ = new EvaluationContext(&fake_clock_);
  }

  virtual void TearDown() {
    eval_ctx_ = NULL;
    // Check that the evaluation context removed all the observers.
    EXPECT_TRUE(fake_int_var_.observer_list_.empty());
    EXPECT_TRUE(fake_async_var_.observer_list_.empty());
    EXPECT_TRUE(fake_const_var_.observer_list_.empty());
    EXPECT_TRUE(fake_poll_var_.observer_list_.empty());
  }

  // TODO(deymo): Update the default timeout to the one passed on construction.
  // See crbug.com/363790
  base::TimeDelta default_timeout_ = base::TimeDelta::FromSeconds(5);

  FakeClock fake_clock_;
  scoped_refptr<EvaluationContext> eval_ctx_;

  // FakeVariables used for testing the EvaluationContext. These are required
  // here to prevent them from going away *before* the EvaluationContext under
  // test does, which keeps a reference to them.
  FakeVariable<int> fake_int_var_ = {"fake_int", kVariableModePoll};
  FakeVariable<string> fake_async_var_ = {"fake_async", kVariableModeAsync};
  FakeVariable<string> fake_const_var_ = {"fake_const", kVariableModeConst};
  FakeVariable<string> fake_poll_var_ = {"fake_poll",
                                         TimeDelta::FromSeconds(1)};
  StrictMock<MockVariable<string>> mock_var_async_{"mock_var_async",
                                                   kVariableModeAsync};
  StrictMock<MockVariable<string>> mock_var_poll_{"mock_var_poll",
                                                  kVariableModePoll};
};

TEST_F(PmEvaluationContextTest, GetValueFails) {
  // FakeVariable is initialized as returning NULL.
  PMTEST_EXPECT_NULL(eval_ctx_->GetValue(&fake_int_var_));
}

TEST_F(PmEvaluationContextTest, GetValueFailsWithInvalidVar) {
  PMTEST_EXPECT_NULL(eval_ctx_->GetValue(
      reinterpret_cast<Variable<int>*>(NULL)));
}

TEST_F(PmEvaluationContextTest, GetValueReturns) {
  const int* p_fake_int;

  fake_int_var_.reset(new int(42));
  p_fake_int = eval_ctx_->GetValue(&fake_int_var_);
  PMTEST_ASSERT_NOT_NULL(p_fake_int);
  EXPECT_EQ(42, *p_fake_int);
}

TEST_F(PmEvaluationContextTest, GetValueCached) {
  const int* p_fake_int;

  fake_int_var_.reset(new int(42));
  p_fake_int = eval_ctx_->GetValue(&fake_int_var_);

  // Check that if the variable changes, the EvaluationContext keeps returning
  // the cached value.
  fake_int_var_.reset(new int(5));

  p_fake_int = eval_ctx_->GetValue(&fake_int_var_);
  PMTEST_ASSERT_NOT_NULL(p_fake_int);
  EXPECT_EQ(42, *p_fake_int);
}

TEST_F(PmEvaluationContextTest, GetValueCachesNull) {
  const int* p_fake_int = eval_ctx_->GetValue(&fake_int_var_);
  PMTEST_EXPECT_NULL(p_fake_int);

  fake_int_var_.reset(new int(42));
  // A second attempt to read the variable should not work because this
  // EvaluationContext already got a NULL value.
  p_fake_int = eval_ctx_->GetValue(&fake_int_var_);
  PMTEST_EXPECT_NULL(p_fake_int);
}

TEST_F(PmEvaluationContextTest, GetValueMixedTypes) {
  const int* p_fake_int;
  const string* p_fake_string;

  fake_int_var_.reset(new int(42));
  fake_poll_var_.reset(new string("Hello world!"));
  // Check that the EvaluationContext can handle multiple Variable types. This
  // is mostly a compile-time check due to the template nature of this method.
  p_fake_int = eval_ctx_->GetValue(&fake_int_var_);
  p_fake_string = eval_ctx_->GetValue(&fake_poll_var_);

  PMTEST_ASSERT_NOT_NULL(p_fake_int);
  EXPECT_EQ(42, *p_fake_int);

  PMTEST_ASSERT_NOT_NULL(p_fake_string);
  EXPECT_EQ("Hello world!", *p_fake_string);
}

// Test that we don't schedule an event if there's no variable to wait for.
TEST_F(PmEvaluationContextTest, RunOnValueChangeOrTimeoutWithoutVariablesTest) {
  fake_const_var_.reset(new string("Hello world!"));
  EXPECT_EQ(*eval_ctx_->GetValue(&fake_const_var_), "Hello world!");

  EXPECT_FALSE(eval_ctx_->RunOnValueChangeOrTimeout(Bind(&DoNothing)));
}

// Test that we don't schedule an event if there's no variable to wait for.
TEST_F(PmEvaluationContextTest, RunOnValueChangeOrTimeoutWithVariablesTest) {
  fake_async_var_.reset(new string("Async value"));
  eval_ctx_->GetValue(&fake_async_var_);

  bool value = false;
  EXPECT_TRUE(eval_ctx_->RunOnValueChangeOrTimeout(Bind(&SetTrue, &value)));
  // Check that the scheduled callback isn't run until we signal a ValueChaged.
  RunGMainLoopMaxIterations(100);
  EXPECT_FALSE(value);

  fake_async_var_.NotifyValueChanged();
  EXPECT_FALSE(value);
  // Ensure that the scheduled callback isn't run until we are back on the main
  // loop.
  RunGMainLoopMaxIterations(100);
  EXPECT_TRUE(value);
}

// Test that we don't re-schedule the events if we are attending one.
TEST_F(PmEvaluationContextTest, RunOnValueChangeOrTimeoutCalledTwiceTest) {
  fake_async_var_.reset(new string("Async value"));
  eval_ctx_->GetValue(&fake_async_var_);

  bool value = false;
  EXPECT_TRUE(eval_ctx_->RunOnValueChangeOrTimeout(Bind(&SetTrue, &value)));
  EXPECT_FALSE(eval_ctx_->RunOnValueChangeOrTimeout(Bind(&SetTrue, &value)));

  // The scheduled event should still work.
  fake_async_var_.NotifyValueChanged();
  RunGMainLoopMaxIterations(100);
  EXPECT_TRUE(value);
}

// Test that we clear the events when destroying the EvaluationContext.
TEST_F(PmEvaluationContextTest, RemoveObserversAndTimeoutTest) {
  fake_async_var_.reset(new string("Async value"));
  eval_ctx_->GetValue(&fake_async_var_);

  bool value = false;
  EXPECT_TRUE(eval_ctx_->RunOnValueChangeOrTimeout(Bind(&SetTrue, &value)));
  eval_ctx_ = NULL;

  // This should not trigger the callback since the EvaluationContext waiting
  // for it is gone, and it should have remove all its observers.
  fake_async_var_.NotifyValueChanged();
  RunGMainLoopMaxIterations(100);
  EXPECT_FALSE(value);
}

// Test that we don't schedule an event if there's no variable to wait for.
TEST_F(PmEvaluationContextTest, RunOnValueChangeOrTimeoutRunsFromTimeoutTest) {
  fake_poll_var_.reset(new string("Polled value"));
  eval_ctx_->GetValue(&fake_poll_var_);

  bool value = false;
  EXPECT_TRUE(eval_ctx_->RunOnValueChangeOrTimeout(Bind(&SetTrue, &value)));
  // Check that the scheduled callback isn't run until the timeout occurs.
  RunGMainLoopMaxIterations(10);
  EXPECT_FALSE(value);
  RunGMainLoopUntil(10000, Bind(&GetBoolean, &value));
  EXPECT_TRUE(value);
}

// Test that we can delete the EvaluationContext while having pending events.
TEST_F(PmEvaluationContextTest, ObjectDeletedWithPendingEventsTest) {
  fake_async_var_.reset(new string("Async value"));
  fake_poll_var_.reset(new string("Polled value"));
  eval_ctx_->GetValue(&fake_async_var_);
  eval_ctx_->GetValue(&fake_poll_var_);
  EXPECT_TRUE(eval_ctx_->RunOnValueChangeOrTimeout(Bind(&DoNothing)));
  // TearDown() checks for leaked observers on this async_variable, which means
  // that our object is still alive after removing its reference.
}

// Test that timed events fired after removal of the EvaluationContext don't
// crash.
TEST_F(PmEvaluationContextTest, TimeoutEventAfterDeleteTest) {
  FakeVariable<string> fake_short_poll_var = {"fake_short_poll", TimeDelta()};
  fake_short_poll_var.reset(new string("Polled value"));
  eval_ctx_->GetValue(&fake_short_poll_var);
  bool value = false;
  EXPECT_TRUE(eval_ctx_->RunOnValueChangeOrTimeout(Bind(&SetTrue, &value)));
  // Remove the last reference to the EvaluationContext and run the loop for
  // 1 second to give time to the main loop to trigger the timeout Event (of 0
  // seconds). Our callback should not be called because the EvaluationContext
  // was removed before the timeout event is attended.
  eval_ctx_ = NULL;
  RunGMainLoopUntil(1000, Bind(&GetBoolean, &value));
  EXPECT_FALSE(value);
}

TEST_F(PmEvaluationContextTest, DefaultTimeout) {
  // Test that the RemainingTime() uses the default timeout on setup.
  EXPECT_CALL(mock_var_async_, GetValue(default_timeout_, _))
      .WillOnce(Return(nullptr));
  PMTEST_EXPECT_NULL(eval_ctx_->GetValue(&mock_var_async_));
}

TEST_F(PmEvaluationContextTest, TimeoutUpdatesWithMonotonicTime) {
  fake_clock_.SetMonotonicTime(
      fake_clock_.GetMonotonicTime() + TimeDelta::FromSeconds(1));

  TimeDelta timeout = default_timeout_ - TimeDelta::FromSeconds(1);

  EXPECT_CALL(mock_var_async_, GetValue(timeout, _))
      .WillOnce(Return(nullptr));
  PMTEST_EXPECT_NULL(eval_ctx_->GetValue(&mock_var_async_));
}

TEST_F(PmEvaluationContextTest, ResetEvaluationResetsTimes) {
  base::Time cur_time = fake_clock_.GetWallclockTime();
  // Advance the time on the clock but don't call ResetEvaluation yet.
  fake_clock_.SetWallclockTime(cur_time + TimeDelta::FromSeconds(4));

  EXPECT_TRUE(eval_ctx_->IsTimeGreaterThan(cur_time -
                                           TimeDelta::FromSeconds(1)));
  EXPECT_FALSE(eval_ctx_->IsTimeGreaterThan(cur_time));
  EXPECT_FALSE(eval_ctx_->IsTimeGreaterThan(cur_time +
                                            TimeDelta::FromSeconds(1)));
  // Call ResetEvaluation now, which should use the new evaluation time.
  eval_ctx_->ResetEvaluation();

  cur_time = fake_clock_.GetWallclockTime();
  EXPECT_TRUE(eval_ctx_->IsTimeGreaterThan(cur_time -
                                           TimeDelta::FromSeconds(1)));
  EXPECT_FALSE(eval_ctx_->IsTimeGreaterThan(cur_time));
  EXPECT_FALSE(eval_ctx_->IsTimeGreaterThan(cur_time +
                                            TimeDelta::FromSeconds(1)));
}

TEST_F(PmEvaluationContextTest, IsTimeGreaterThanSignalsTriggerReevaluation) {
  EXPECT_FALSE(eval_ctx_->IsTimeGreaterThan(
      fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(1)));

  // The "false" from IsTimeGreaterThan means that's not that timestamp yet, so
  // this should schedule a callback for when that happens.
  EXPECT_TRUE(eval_ctx_->RunOnValueChangeOrTimeout(Bind(&DoNothing)));
}

TEST_F(PmEvaluationContextTest, IsTimeGreaterThanDoesntRecordPastTimestamps) {
  // IsTimeGreaterThan() should ignore timestamps on the past for reevaluation.
  EXPECT_TRUE(eval_ctx_->IsTimeGreaterThan(
      fake_clock_.GetWallclockTime() - TimeDelta::FromSeconds(20)));
  EXPECT_TRUE(eval_ctx_->IsTimeGreaterThan(
      fake_clock_.GetWallclockTime() - TimeDelta::FromSeconds(1)));

  // Callback should not be scheduled.
  EXPECT_FALSE(eval_ctx_->RunOnValueChangeOrTimeout(Bind(&DoNothing)));
}

}  // namespace chromeos_policy_manager
