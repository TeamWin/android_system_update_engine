// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/variable.h"

#include <vector>

#include <gtest/gtest.h>

#include "update_engine/test_utils.h"

using base::TimeDelta;
using chromeos_update_engine::test_utils::RunGMainLoopMaxIterations;
using std::string;
using std::vector;

namespace chromeos_update_manager {

// Variable class that returns a value constructed with the default value.
template <typename T>
class DefaultVariable : public Variable<T> {
 public:
  DefaultVariable(const string& name, VariableMode mode)
      : Variable<T>(name, mode) {}
  DefaultVariable(const string& name, const TimeDelta& poll_interval)
      : Variable<T>(name, poll_interval) {}
  ~DefaultVariable() override {}

 protected:
  const T* GetValue(TimeDelta /* timeout */,
                    string* /* errmsg */) override {
    return new T();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultVariable);
};

TEST(UmBaseVariableTest, GetNameTest) {
  DefaultVariable<int> var("var", kVariableModeConst);
  EXPECT_EQ(var.GetName(), string("var"));
}

TEST(UmBaseVariableTest, GetModeTest) {
  DefaultVariable<int> var("var", kVariableModeConst);
  EXPECT_EQ(var.GetMode(), kVariableModeConst);
  DefaultVariable<int> other_var("other_var", kVariableModePoll);
  EXPECT_EQ(other_var.GetMode(), kVariableModePoll);
}

TEST(UmBaseVariableTest, DefaultPollIntervalTest) {
  DefaultVariable<int> const_var("const_var", kVariableModeConst);
  EXPECT_EQ(const_var.GetPollInterval(), TimeDelta());
  DefaultVariable<int> poll_var("poll_var", kVariableModePoll);
  EXPECT_EQ(poll_var.GetPollInterval(), TimeDelta::FromMinutes(5));
}

TEST(UmBaseVariableTest, GetPollIntervalTest) {
  DefaultVariable<int> var("var", TimeDelta::FromMinutes(3));
  EXPECT_EQ(var.GetMode(), kVariableModePoll);
  EXPECT_EQ(var.GetPollInterval(), TimeDelta::FromMinutes(3));
}

class BaseVariableObserver : public BaseVariable::ObserverInterface {
 public:
  void ValueChanged(BaseVariable* variable) {
    calls_.push_back(variable);
  }

  // List of called functions.
  vector<BaseVariable*> calls_;
};

TEST(UmBaseVariableTest, RepeatedObserverTest) {
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

TEST(UmBaseVariableTest, NotifyValueChangedTest) {
  DefaultVariable<int> var("var", kVariableModeAsync);
  BaseVariableObserver observer1;
  var.AddObserver(&observer1);
  // Simulate a value change on the variable's implementation.
  var.NotifyValueChanged();
  ASSERT_EQ(0, observer1.calls_.size());
  RunGMainLoopMaxIterations(100);

  ASSERT_EQ(1, observer1.calls_.size());
  // Check that the observer is called with the right argument.
  EXPECT_EQ(&var, observer1.calls_[0]);

  BaseVariableObserver observer2;
  var.AddObserver(&observer2);
  var.NotifyValueChanged();
  RunGMainLoopMaxIterations(100);

  // Check that all the observers are called.
  EXPECT_EQ(2, observer1.calls_.size());
  EXPECT_EQ(1, observer2.calls_.size());

  var.RemoveObserver(&observer1);
  var.RemoveObserver(&observer2);
}

class BaseVariableObserverRemover : public BaseVariable::ObserverInterface {
 public:
  BaseVariableObserverRemover() : calls_(0) {}

  void ValueChanged(BaseVariable* variable) override {
    for (auto& observer : remove_observers_) {
      variable->RemoveObserver(observer);
    }
    calls_++;
  }

  void OnCallRemoveObserver(BaseVariable::ObserverInterface* observer) {
    remove_observers_.push_back(observer);
  }

  int get_calls() { return calls_; }

 private:
  vector<BaseVariable::ObserverInterface*> remove_observers_;
  int calls_;
};

// Tests that we can remove an observer from a Variable on the ValueChanged()
// call to that observer.
TEST(UmBaseVariableTest, NotifyValueRemovesObserversTest) {
  DefaultVariable<int> var("var", kVariableModeAsync);
  BaseVariableObserverRemover observer1;
  BaseVariableObserverRemover observer2;

  var.AddObserver(&observer1);
  var.AddObserver(&observer2);

  // Make each observer remove both observers on ValueChanged.
  observer1.OnCallRemoveObserver(&observer1);
  observer1.OnCallRemoveObserver(&observer2);
  observer2.OnCallRemoveObserver(&observer1);
  observer2.OnCallRemoveObserver(&observer2);

  var.NotifyValueChanged();
  RunGMainLoopMaxIterations(100);

  EXPECT_EQ(1, observer1.get_calls() + observer2.get_calls());
}

}  // namespace chromeos_update_manager
