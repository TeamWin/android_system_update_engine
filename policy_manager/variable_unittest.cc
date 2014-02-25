// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "update_engine/policy_manager/variable.h"

using base::TimeDelta;
using std::string;

namespace chromeos_policy_manager {

// Variable class that returns a value constructed with the default value.
template <typename T>
class DefaultVariable : public Variable<T> {
 public:
  DefaultVariable(const string& name, VariableMode mode)
      : Variable<T>(name, mode) {}
  DefaultVariable(const string& name, const TimeDelta& poll_interval)
      : Variable<T>(name, poll_interval) {}
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

TEST(PmBaseVariableTest, DefaultPollIntervalTest) {
  DefaultVariable<int> const_var("const_var", kVariableModeConst);
  EXPECT_EQ(const_var.GetPollInterval(), TimeDelta());
  DefaultVariable<int> poll_var("poll_var", kVariableModePoll);
  EXPECT_EQ(poll_var.GetPollInterval(), TimeDelta::FromMinutes(5));
}

TEST(PmBaseVariableTest, GetPollIntervalTest) {
  DefaultVariable<int> var("var", TimeDelta::FromMinutes(3));
  EXPECT_EQ(var.GetMode(), kVariableModePoll);
  EXPECT_EQ(var.GetPollInterval(), TimeDelta::FromMinutes(3));
}

}  // namespace chromeos_policy_manager
