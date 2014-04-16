// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generic and provider-independent Variable subclasses. These variables can be
// used by any state provider to implement simple variables to avoid repeat the
// same common code on different state providers.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_GENERIC_VARIABLES_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_GENERIC_VARIABLES_H_

#include "update_engine/policy_manager/variable.h"

namespace {

const char* kCopyVariableDefaultErrMsg = "Requested value is not set";

}  // namespace


namespace chromeos_policy_manager {

// Variable class returning a copy of a given object using the copy constructor.
// This template class can be used to define variables that expose as a variable
// any fixed object, such as the a provider's private member. The variable will
// create copies of the provided object using the copy constructor of that
// class.
//
// For example, a state provider exposing a private member as a variable can
// implement this as follows:
//
//   class SomethingProvider {
//    public:
//      SomethingProvider(...) {
//        var_something_foo = new CopyVariable<MyType>(foo_);
//      }
//      ...
//    private:
//     MyType foo_;
//   };
template<typename T>
class CopyVariable : public Variable<T> {
 public:
  // Creates the variable returning copies of the passed |ref|. The reference to
  // this object is kept and it should be available whenever the GetValue()
  // method is called. If |is_set_p| is not null, then this flag will be
  // consulted prior to returning the value, and an |errmsg| will be returned if
  // it is not set.
  CopyVariable(const std::string& name, VariableMode mode, const T& ref,
               const bool* is_set_p, const std::string& errmsg)
      : Variable<T>(name, mode), ref_(ref), is_set_p_(is_set_p),
        errmsg_(errmsg) {}
  CopyVariable(const std::string& name, VariableMode mode, const T& ref,
               const bool* is_set_p)
      : CopyVariable(name, mode, ref, is_set_p, kCopyVariableDefaultErrMsg) {}
  CopyVariable(const std::string& name, VariableMode mode, const T& ref)
      : CopyVariable(name, mode, ref, nullptr) {}

  CopyVariable(const std::string& name, const base::TimeDelta poll_interval,
               const T& ref, const bool* is_set_p, const std::string& errmsg)
      : Variable<T>(name, poll_interval), ref_(ref), is_set_p_(is_set_p),
        errmsg_(errmsg) {}
  CopyVariable(const std::string& name, const base::TimeDelta poll_interval,
               const T& ref, const bool* is_set_p)
      : CopyVariable(name, poll_interval, ref, is_set_p,
                     kCopyVariableDefaultErrMsg) {}
  CopyVariable(const std::string& name, const base::TimeDelta poll_interval,
               const T& ref)
      : CopyVariable(name, poll_interval, ref, nullptr) {}

 protected:
  friend class PmCopyVariableTest;
  FRIEND_TEST(PmCopyVariableTest, SimpleTest);
  FRIEND_TEST(PmCopyVariableTest, SetFlagTest);
  FRIEND_TEST(PmCopyVariableTest, UseCopyConstructorTest);

  // Variable override.
  virtual inline const T* GetValue(base::TimeDelta /* timeout */,
                                   std::string* errmsg) {
    if (is_set_p_ && !(*is_set_p_)) {
      if (errmsg)
        *errmsg = errmsg_;
      return nullptr;
    }
    return new T(ref_);
  }

 private:
  // Reference to the object to be copied by GetValue().
  const T& ref_;

  // A pointer to a flag indicating whether the value is set. If null, then the
  // value is assumed to be set.
  const bool* const is_set_p_;

  // An error message to be returned when attempting to get an unset value.
  const std::string errmsg_;
};

// Variable class returning a constant value that is cached on the variable when
// it is created.
template<typename T>
class ConstCopyVariable : public Variable<T> {
 public:
  // Creates the variable returning copies of the passed |obj|. The value passed
  // is copied in this variable, and new copies of it will be returned by
  // GetValue().
  ConstCopyVariable(const std::string& name, const T& obj)
      : Variable<T>(name, kVariableModeConst), obj_(obj) {}

 protected:
  friend class PmConstCopyVariableTest;
  FRIEND_TEST(PmConstCopyVariableTest, SimpleTest);

  // Variable override.
  virtual const T* GetValue(base::TimeDelta /* timeout */,
                            std::string* /* errmsg */) {
    return new T(obj_);
  }

 private:
  // Value to be copied by GetValue().
  const T obj_;
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_GENERIC_VARIABLES_H_
