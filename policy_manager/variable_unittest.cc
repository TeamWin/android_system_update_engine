// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "update_engine/policy_manager/variable.h"

using std::string;

namespace chromeos_policy_manager {

// Variable class that returns a value constructed with the default value.
template <typename T>
class DefaultVariable : public Variable<T> {
 public:
  DefaultVariable(const string& name, VariableMode mode)
      : Variable<T>(name, mode) {}
  virtual ~DefaultVariable() {}

 protected:
  virtual const T* GetValue(base::TimeDelta /* timeout */,
                            string* /* errmsg */) {
    return new T();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultVariable);
};

TEST(PmBaseVariableTest, GetNameTest) {
  DefaultVariable<int> var("var", kVariableModeConst);
  EXPECT_EQ(var.GetName(), string("var"));
}

TEST(PmBaseVariableTest, GetModeTest) {
  DefaultVariable<int> var("var", kVariableModeConst);
  EXPECT_EQ(var.GetMode(), kVariableModeConst);
  DefaultVariable<int> other_var("other_var", kVariableModePoll);
  EXPECT_EQ(other_var.GetMode(), kVariableModePoll);
}

}  // namespace chromeos_policy_manager
