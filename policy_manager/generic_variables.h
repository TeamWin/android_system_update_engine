// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generic and provider independent Variable subclasses. These variables can be
// used by any state provider to implement simple variables to avoid repeat the
// same common code on different state providers.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_GENERIC_VARIABLES_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_GENERIC_VARIABLES_H

#include "policy_manager/variable.h"

namespace chromeos_policy_manager {

// Variable class returning a copy of a given object using the copy constructor.
// This template class can be used to define variables that expose as a variable
// any fixed object, such as the a provider's private member. The variable will
// create copies of the provided object using the copy constructor of that
// class.
//
// For example, a state provider exposing a private method as a variable could
// be implemented in this way:
//
// * On something_vars.h:
//   Variable<MyType>* var_something;
//
// * On something_provider:
//   class SomethingProvider {
//    public:
//      SomethingProvider(...) {
//        var_something = new CopyVariable<MyType>(priv_object_);
//      }
//      ~SomethingProvider() {
//        delete var_something;
//        var_something = NULL;
//      }
//    private:
//     MyType priv_object_;
//   };
template<typename T>
class CopyVariable : public Variable<T> {
 public:
  // Creates the variable returning copies of the passed |obj| reference. The
  // reference to this object is kept and it should be available whenever the
  // GetValue() method is called.
  CopyVariable(const T& obj);

  virtual ~CopyVariable() {}

 protected:
  friend class PMCopyVariableTest;
  FRIEND_TEST(PMCopyVariableTest, SimpleTest);
  FRIEND_TEST(PMCopyVariableTest, UseCopyConstructorTest);

  // Variable override.
  virtual const T* GetValue(base::TimeDelta timeout, std::string* errmsg);

 private:
  // Reference to the object to be copied by GetValue().
  const T& ref_;
};

}  // namespace chromeos_policy_manager

// Include implementation on header files for templates.
#include "policy_manager/generic_variables-inl.h"

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_GENERIC_VARIABLES_H
