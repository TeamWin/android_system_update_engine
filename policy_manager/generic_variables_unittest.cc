// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/generic_variables.h"

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/test_utils.h"

using base::TimeDelta;
using chromeos_update_engine::RunGMainLoopMaxIterations;

namespace chromeos_policy_manager {

class PmCopyVariableTest : public ::testing::Test {};


TEST_F(PmCopyVariableTest, SimpleTest) {
  // Tests that copies are generated as intended.
  int source = 5;
  CopyVariable<int> var("var", kVariableModePoll, source);

  // Generate and validate a copy.
  scoped_ptr<const int> copy_1(var.GetValue(
          PmTestUtils::DefaultTimeout(), NULL));
  PMTEST_ASSERT_NOT_NULL(copy_1.get());
  EXPECT_EQ(5, *copy_1);

  // Assign a different value to the source variable.
  source = 42;

  // Check that the content of the copy was not affected (distinct instance).
  EXPECT_EQ(5, *copy_1);

  // Generate and validate a second copy.
  PmTestUtils::ExpectVariableHasValue(42, &var);
}

TEST_F(PmCopyVariableTest, SetFlagTest) {
  // Tests that the set flag is being referred to as expected.
  int source = 5;
  bool is_set = false;
  CopyVariable<int> var("var", kVariableModePoll, source, &is_set);

  // Flag marked unset, nothing should be returned.
  PmTestUtils::ExpectVariableNotSet(&var);

  // Flag marked set, we should be getting a value.
  is_set = true;
  PmTestUtils::ExpectVariableHasValue(5, &var);
}


class CopyConstructorTestClass {
 public:
  CopyConstructorTestClass(void) : copied_(false) {}
  CopyConstructorTestClass(const CopyConstructorTestClass& /* ignored */)
      : copied_(true) {}

  // Tells if the instance was constructed using the copy-constructor.
  bool copied_;
};


TEST_F(PmCopyVariableTest, UseCopyConstructorTest) {
  // Ensures that CopyVariables indeed uses the copy contructor.
  const CopyConstructorTestClass source;
  ASSERT_FALSE(source.copied_);

  CopyVariable<CopyConstructorTestClass> var("var", kVariableModePoll, source);
  scoped_ptr<const CopyConstructorTestClass> copy(
      var.GetValue(PmTestUtils::DefaultTimeout(), NULL));
  PMTEST_ASSERT_NOT_NULL(copy.get());
  EXPECT_TRUE(copy->copied_);
}


class PmConstCopyVariableTest : public ::testing::Test {};

TEST_F(PmConstCopyVariableTest, SimpleTest) {
  int source = 5;
  ConstCopyVariable<int> var("var", source);
  PmTestUtils::ExpectVariableHasValue(5, &var);

  // Ensure the value is cached.
  source = 42;
  PmTestUtils::ExpectVariableHasValue(5, &var);
}


class PmAsyncCopyVariableTest : public ::testing::Test {
 public:
  void TearDown() {
    // No remaining event on the main loop.
    EXPECT_EQ(0, RunGMainLoopMaxIterations(1));
  }
};

TEST_F(PmAsyncCopyVariableTest, ConstructorTest) {
  AsyncCopyVariable<int> var("var");
  PmTestUtils::ExpectVariableNotSet(&var);
  EXPECT_EQ(kVariableModeAsync, var.GetMode());
}

TEST_F(PmAsyncCopyVariableTest, SetValueTest) {
  AsyncCopyVariable<int> var("var");
  var.SetValue(5);
  PmTestUtils::ExpectVariableHasValue(5, &var);
  // Execute all the pending observers.
  RunGMainLoopMaxIterations(100);
}

TEST_F(PmAsyncCopyVariableTest, UnsetValueTest) {
  AsyncCopyVariable<int> var("var", 42);
  var.UnsetValue();
  PmTestUtils::ExpectVariableNotSet(&var);
  // Execute all the pending observers.
  RunGMainLoopMaxIterations(100);
}

class CallCounterObserver : public BaseVariable::ObserverInterface {
 public:
  void ValueChanged(BaseVariable* variable) {
    calls_count_++;
  }

  int calls_count_ = 0;
};

TEST_F(PmAsyncCopyVariableTest, ObserverCalledTest) {
  AsyncCopyVariable<int> var("var", 42);
  CallCounterObserver observer;
  var.AddObserver(&observer);
  EXPECT_EQ(0, observer.calls_count_);

  // Check that a different value fires the notification.
  var.SetValue(5);
  RunGMainLoopMaxIterations(100);
  EXPECT_EQ(1, observer.calls_count_);

  // Check the same value doesn't.
  var.SetValue(5);
  RunGMainLoopMaxIterations(100);
  EXPECT_EQ(1, observer.calls_count_);

  // Check that unsetting a previously set value fires the notification.
  var.UnsetValue();
  RunGMainLoopMaxIterations(100);
  EXPECT_EQ(2, observer.calls_count_);

  // Check that unsetting again doesn't.
  var.UnsetValue();
  RunGMainLoopMaxIterations(100);
  EXPECT_EQ(2, observer.calls_count_);

  var.RemoveObserver(&observer);
}

}  // namespace chromeos_policy_manager
