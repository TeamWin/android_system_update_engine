// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H

#include <string>

#include <base/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace chromeos_policy_manager {

// This class is a base class with the common functionality that doesn't
// deppend on the variable's type, implemented by all the variables.
class BaseVariable {
 public:
  BaseVariable(const std::string& name) : name_(name) {}
  virtual ~BaseVariable() {}

  // Returns the variable name as a string.
  virtual const std::string& GetName() {
    return name_;
  }

 private:
  // The variable's name as a string.
  const std::string name_;
};

// Interface to a Policy Manager variable of a given type. Implementation
// internals are hidden as protected members, since policies should not be
// using them directly.
template<typename T>
class Variable : public BaseVariable {
 public:
  Variable(const std::string& name) : BaseVariable(name) {}
  virtual ~Variable() {}

 protected:
  friend class PmRealRandomProviderTest;
  FRIEND_TEST(PmRealRandomProviderTest, GetRandomValues);

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
