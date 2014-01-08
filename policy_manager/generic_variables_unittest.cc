// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include "policy_manager/generic_variables.h"

using base::TimeDelta;
using std::string;

namespace chromeos_policy_manager {

TEST(PMCopyVariableTest, SimpleTest) {
  int obj_int = 5;

  CopyVariable<int> var(obj_int);

  string errmsg = "Nope";

  const int* res_1 = var.GetValue(TimeDelta::FromSeconds(1), &errmsg);
  EXPECT_NE(res_1, static_cast<void*>(NULL));
  EXPECT_EQ(5, *res_1);

  obj_int = 42;

  // Check the result in res_1 is actually a new copy.
  EXPECT_EQ(5, *res_1);

  const int* res_2 = var.GetValue(TimeDelta::FromSeconds(1), &errmsg);
  EXPECT_NE(res_2, static_cast<void*>(NULL));
  EXPECT_EQ(42, *res_2);

  delete res_1;
  delete res_2;
}

class ConstructorTestClass {
 public:
  ConstructorTestClass(void) : copied_(false) {}

  ConstructorTestClass(const ConstructorTestClass& /* ignored */)
      : copied_(true) {}

  // Tells if the instance was constructed using the copy-constructor.
  bool copied_;
};

TEST(PMCopyVariableTest, UseCopyConstructorTest) {
  ConstructorTestClass obj;
  ASSERT_FALSE(obj.copied_);

  string errmsg;
  CopyVariable<ConstructorTestClass> var(obj);
  const ConstructorTestClass* value =
      var.GetValue(TimeDelta::FromSeconds(1), &errmsg);
  EXPECT_NE(value, static_cast<void*>(NULL));
  EXPECT_TRUE(value->copied_);

  delete value;
}

}  // namespace chromeos_policy_manager
