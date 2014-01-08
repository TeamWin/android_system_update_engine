// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_GENERIC_VARIABLES_INL_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_GENERIC_VARIABLES_INL_H

namespace chromeos_policy_manager {

template<typename T>
CopyVariable<T>::CopyVariable(const T& ref) : ref_(ref) {}

template<typename T>
const T* CopyVariable<T>::GetValue(base::TimeDelta /* timeout */,
                                   std::string* /* errmsg */) {
  return new T(ref_);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_GENERIC_VARIABLES_INL_H
