// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "policy_manager/generic_variables.h"
#include "policy_manager/pmtest_utils.h"

using base::TimeDelta;

namespace chromeos_policy_manager {

class PmCopyVariableTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    default_timeout_ = TimeDelta::FromSeconds(1);
  }

  TimeDelta default_timeout_;
};


TEST_F(PmCopyVariableTest, SimpleTest) {
  // Tests that copies are generated as intended.
  int source = 5;
  CopyVariable<int> var("var", source);

  // Generate and validate a copy.
  scoped_ptr<const int> copy_1(var.GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(copy_1.get());
  EXPECT_EQ(5, *copy_1);

  // Assign a different value to the source variable.
  source = 42;

  // Check that the content of the copy was not affected (distinct instance).
  EXPECT_EQ(5, *copy_1);

  // Generate and validate a second copy.
  scoped_ptr<const int> copy_2(var.GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(copy_2.get());
  EXPECT_EQ(42, *copy_2);
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

  CopyVariable<CopyConstructorTestClass> var("var", source);
  scoped_ptr<const CopyConstructorTestClass> copy(
      var.GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(copy.get());
  EXPECT_TRUE(copy->copied_);
}

}  // namespace chromeos_policy_manager
