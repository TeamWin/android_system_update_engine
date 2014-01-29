// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PROVIDER_UTILS_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PROVIDER_UTILS_H

namespace chromeos_policy_manager {

// Scoped closer for a pointer variable. It is initialized with a reference to
// a pointer variable. Upon destruction, it will destruct the object pointed to
// by the variable and nullify the variable. This template can be easily
// instantiated via 'typeof' of the variable that is being scoped:
//
//   ScopedPtrVarCloser<typeof(foo)> foo_closer(&foo);
//
// where 'foo' is pointer variable of some type.
template<typename T>
class ScopedPtrVarCloser {
 public:
  ScopedPtrVarCloser(T* ptr_var_p) : ptr_var_p_(ptr_var_p) {}
  ~ScopedPtrVarCloser() {
    if (ptr_var_p_) {
      delete *ptr_var_p_;
      *ptr_var_p_ = NULL;
    }
  }

 private:
  T* ptr_var_p_;
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PROVIDER_UTILS_H
