// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H_

#include <algorithm>
#include <list>
#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <base/time/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/policy_manager/event_loop.h"

namespace chromeos_policy_manager {

// The VariableMode specifies important behavior of the variable in terms of
// whether, how and when the value of the variable changes.
enum VariableMode {
  // Const variables never changes during the life of a policy request, so the
  // EvaluationContext caches the value even between different evaluations of
  // the same policy request.
  kVariableModeConst,

  // Poll variables, or synchronous variables, represent a variable with a value
  // that can be queried at any time, but it is not known when the value
  // changes on the source of information. In order to detect if the value of
  // the variable changes, it has to be queried again.
  kVariableModePoll,

  // Async variables are able to produce a signal or callback whenever the
  // value changes. This means that it's not required to poll the value to
  // detect when it changes, instead, you should register an observer to get
  // a notification when that happens.
  kVariableModeAsync,
};

// This class is a base class with the common functionality that doesn't
// deppend on the variable's type, implemented by all the variables.
class BaseVariable {
 public:
  // Interface for observing changes on variable value.
  class ObserverInterface {
   public:
    virtual ~ObserverInterface() {}

    // Called when the value on the variable changes.
    virtual void ValueChanged(BaseVariable* variable) = 0;
  };

  virtual ~BaseVariable() {
    if (!observer_list_.empty()) {
      LOG(WARNING) << "Variable " << name_ << " deleted with "
                   << observer_list_.size() << " observers.";
    }
    DCHECK(observer_list_.empty()) << "Don't destroy the variable without "
                                      "removing the observers.";
  }

  // Returns the variable name as a string.
  const std::string& GetName() const {
    return name_;
  }

  // Returns the variable mode.
  VariableMode GetMode() const {
    return mode_;
  }

  // For VariableModePoll variables, it returns the polling interval of this
  // variable. In other case, it returns 0.
  base::TimeDelta GetPollInterval() const {
    return poll_interval_;
  }

  // Adds and removes observers for value changes on the variable. This only
  // works for kVariableAsync variables since the other modes don't track value
  // changes. Adding the same observer twice has no effect.
  virtual void AddObserver(BaseVariable::ObserverInterface* observer) {
    if (std::find(observer_list_.begin(), observer_list_.end(), observer) ==
        observer_list_.end()) {
      observer_list_.push_back(observer);
    }
  }

  virtual void RemoveObserver(BaseVariable::ObserverInterface* observer) {
    observer_list_.remove(observer);
  }

 protected:
  // Creates a BaseVariable using the default polling interval (5 minutes).
  BaseVariable(const std::string& name, VariableMode mode)
      : BaseVariable(name, mode,
                     base::TimeDelta::FromMinutes(kDefaultPollMinutes)) {}

  // Creates a BaseVariable with mode kVariableModePoll and the provided
  // polling interval.
  BaseVariable(const std::string& name, base::TimeDelta poll_interval)
      : BaseVariable(name, kVariableModePoll, poll_interval) {}

  // Calls ValueChanged on all the observers.
  void NotifyValueChanged() {
    // Fire all the observer methods from the main loop as single call. In order
    // to avoid scheduling these callbacks when it is not needed, we check
    // first the list of observers.
    if (!observer_list_.empty()) {
        RunFromMainLoop(base::Bind(&BaseVariable::OnValueChangedNotification,
                                  base::Unretained(this)));
    }
  }

 private:
  friend class PmEvaluationContextTest;
  friend class PmBaseVariableTest;
  FRIEND_TEST(PmBaseVariableTest, RepeatedObserverTest);
  FRIEND_TEST(PmBaseVariableTest, NotifyValueChangedTest);
  FRIEND_TEST(PmBaseVariableTest, NotifyValueRemovesObserversTest);

  BaseVariable(const std::string& name, VariableMode mode,
               base::TimeDelta poll_interval)
    : name_(name), mode_(mode),
      poll_interval_(mode == kVariableModePoll ?
                     poll_interval : base::TimeDelta()) {}

  void OnValueChangedNotification() {
    // A ValueChanged() method can change the list of observers, for example
    // removing itself and invalidating the iterator, so we create a snapshot
    // of the observers first. Also, to support the case when *another* observer
    // is removed, we check for them.
    std::list<BaseVariable::ObserverInterface*> observer_list_copy(
        observer_list_);

    for (auto& observer : observer_list_copy) {
      if (std::find(observer_list_.begin(), observer_list_.end(), observer) !=
          observer_list_.end()) {
        observer->ValueChanged(this);
      }
    }
  }

  // The default PollInterval in minutes.
  static constexpr int kDefaultPollMinutes = 5;

  // The variable's name as a string.
  const std::string name_;

  // The variable's mode.
  const VariableMode mode_;

  // The variable's polling interval for VariableModePoll variable and 0 for
  // other modes.
  const base::TimeDelta poll_interval_;

  // The list of value changes observers.
  std::list<BaseVariable::ObserverInterface*> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(BaseVariable);
};

// Interface to a Policy Manager variable of a given type. Implementation
// internals are hidden as protected members, since policies should not be
// using them directly.
template<typename T>
class Variable : public BaseVariable {
 public:
  virtual ~Variable() {}

 protected:
  // Only allow to get values through the EvaluationContext class and not
  // directly from the variable.
  friend class EvaluationContext;

  friend class PmRealDevicePolicyProviderTest;
  FRIEND_TEST(PmRealDevicePolicyProviderTest,
              NonExistentDevicePolicyEmptyVariables);
  friend class PmRealRandomProviderTest;
  FRIEND_TEST(PmRealRandomProviderTest, GetRandomValues);
  friend class PmRealShillProviderTest;
  FRIEND_TEST(PmRealShillProviderTest, ReadBaseValues);
  FRIEND_TEST(PmRealShillProviderTest, ReadConnTypeVpn);
  FRIEND_TEST(PmRealShillProviderTest, ReadLastChangedTimeTwoSignals);
  FRIEND_TEST(PmRealShillProviderTest, ConnTypeCacheUsed);
  FRIEND_TEST(PmRealShillProviderTest, ConnTypeCacheRemainsValid);
  FRIEND_TEST(PmRealShillProviderTest, ConnTetheringCacheUsed);
  FRIEND_TEST(PmRealShillProviderTest, ConnTetheringCacheRemainsValid);
  FRIEND_TEST(PmRealShillProviderTest, NoInitConnStatusReadBaseValues);
  friend class PmRealTimeProviderTest;
  FRIEND_TEST(PmRealTimeProviderTest, CurrDateValid);
  FRIEND_TEST(PmRealTimeProviderTest, CurrHourValid);
  friend class PmRealUpdaterProviderTest;

  Variable(const std::string& name, VariableMode mode)
      : BaseVariable(name, mode) {}

  Variable(const std::string& name, const base::TimeDelta poll_interval)
      : BaseVariable(name, poll_interval) {}

  // Gets the current value of the variable. The current value is copied to a
  // new object and returned. The caller of this method owns the object and
  // should delete it.
  //
  // In case of and error getting the current value or the |timeout| timeout is
  // exceeded, a NULL value is returned and the |errmsg| is set.
  //
  // The caller can pass a NULL value for |errmsg|, in which case the error
  // message won't be set.
  virtual const T* GetValue(base::TimeDelta timeout, std::string* errmsg) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Variable);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H_
