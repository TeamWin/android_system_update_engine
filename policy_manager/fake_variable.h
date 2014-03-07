// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_VARIABLE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_VARIABLE_H_

#include <base/memory/scoped_ptr.h>

#include "update_engine/policy_manager/variable.h"

namespace chromeos_policy_manager {

// A fake typed variable to use while testing policy implementations. The
// variable can be instructed to return any object of its type.
template<typename T>
class FakeVariable : public Variable<T> {
 public:
  explicit FakeVariable(const std::string& name, VariableMode mode)
      : Variable<T>(name, mode) {}
  virtual ~FakeVariable() {}

  // Sets the next value of this variable to the passed |p_value| pointer. Once
  // returned by GetValue(), the pointer is released and has to be set again.
  // A value of NULL means that the GetValue() call will fail and return NULL.
  void reset(const T* p_value) {
    ptr_.reset(p_value);
  }

 protected:
  // Variable<T> overrides.
  // Returns the pointer set with reset(). The ownership of the object is passed
  // to the caller and the pointer is release from the FakeVariable. A second
  // call to GetValue() without reset() will return NULL and set the error
  // message.
  virtual const T* GetValue(base::TimeDelta /* timeout */,
                            std::string* errmsg) {
    if (ptr_ == NULL && errmsg != NULL)
      *errmsg = this->GetName() + " is an empty FakeVariable";
    // Passes the pointer ownership to the caller.
    return ptr_.release();
  }

 private:
  // The pointer returned by GetValue().
  scoped_ptr<const T> ptr_;
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_VARIABLE_H_
