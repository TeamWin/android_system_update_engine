// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <gtest/gtest.h>

#include "update_engine/policy_manager/variable.h"

using base::TimeDelta;
using std::string;
using std::vector;

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

class BaseVariableObserver : public BaseVariable::Observer {
 public:
  void ValueChanged(BaseVariable* variable) {
    calls_.push_back(variable);
  }

  // List of called functions.
  vector<BaseVariable*> calls_;
};

TEST(PmBaseVariableTest, RepeatedObserverTest) {
  DefaultVariable<int> var("var", kVariableModeAsync);
  BaseVariableObserver observer;
  var.AddObserver(&observer);
  EXPECT_EQ(var.observer_list_.size(), 1);
  var.AddObserver(&observer);
  EXPECT_EQ(var.observer_list_.size(), 1);
  var.RemoveObserver(&observer);
  EXPECT_EQ(var.observer_list_.size(), 0);
  var.RemoveObserver(&observer);
  EXPECT_EQ(var.observer_list_.size(), 0);
}

TEST(PmBaseVariableTest, NotifyValueChangedTest) {
  DefaultVariable<int> var("var", kVariableModeAsync);
  BaseVariableObserver observer1;
  var.AddObserver(&observer1);
  // Simulate a value change on the variable's implementation.
  var.NotifyValueChanged();

  ASSERT_EQ(observer1.calls_.size(), 1);
  // Check that the observer is called with the right argument.
  EXPECT_EQ(observer1.calls_[0], &var);

  BaseVariableObserver observer2;
  var.AddObserver(&observer2);
  var.NotifyValueChanged();

  // Check that all the observers are called.
  EXPECT_EQ(observer1.calls_.size(), 2);
  EXPECT_EQ(observer2.calls_.size(), 1);
}

}  // namespace chromeos_policy_manager
