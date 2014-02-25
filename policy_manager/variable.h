// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H

#include <string>

#include <base/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

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
  virtual ~BaseVariable() {}

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

 protected:
  // Creates a BaseVariable using the default polling interval (5 minutes).
  BaseVariable(const std::string& name, VariableMode mode)
      : BaseVariable(name, mode,
                     base::TimeDelta::FromMinutes(kDefaultPollMinutes)) {}

  // Creates a BaseVariable with mode kVariableModePoll and the provided
  // polling interval.
  BaseVariable(const std::string& name, base::TimeDelta poll_interval)
      : BaseVariable(name, kVariableModePoll, poll_interval) {}

 private:
  BaseVariable(const std::string& name, VariableMode mode,
               base::TimeDelta poll_interval)
    : name_(name), mode_(mode),
      poll_interval_(mode == kVariableModePoll ?
                     poll_interval : base::TimeDelta()) {}

  // The default PollInterval in minutes.
  static constexpr int kDefaultPollMinutes = 5;

  // The variable's name as a string.
  const std::string name_;

  // The variable's mode.
  const VariableMode mode_;

  // The variable's polling interval for VariableModePoll variable and 0 for
  // other modes.
  const base::TimeDelta poll_interval_;
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

  friend class PmRealRandomProviderTest;
  FRIEND_TEST(PmRealRandomProviderTest, GetRandomValues);
  friend class PmRealShillProviderTest;
  FRIEND_TEST(PmRealShillProviderTest, DefaultValues);

  Variable(const std::string& name, VariableMode mode)
      : BaseVariable(name, mode) {}

  Variable(const std::string& name, const base::TimeDelta& poll_interval)
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
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H
