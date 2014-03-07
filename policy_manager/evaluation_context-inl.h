// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_EVALUATION_CONTEXT_INL_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_EVALUATION_CONTEXT_INL_H_

#include <base/logging.h>

namespace chromeos_policy_manager {

template<typename T>
const T* EvaluationContext::GetValue(Variable<T>* var) {
  if (var == NULL) {
    LOG(ERROR) << "GetValue received an uninitialized variable.";
    return NULL;
  }

  // Search for the value on the cache first.
  ValueCacheMap::iterator it = value_cache_.find(var);
  if (it != value_cache_.end())
    return reinterpret_cast<const T*>(it->second.value());

  // Get the value from the variable if not found on the cache.
  std::string errmsg;
  const T* result = var->GetValue(RemainingTime(), &errmsg);
  if (result == NULL) {
    LOG(WARNING) << "Error reading Variable " << var->GetName() << ": \""
        << errmsg << "\"";
  } else {
    // Cache the value for the next time. The map of CachedValues keeps the
    // ownership of the pointer until the map is destroyed.
    value_cache_.emplace(
      static_cast<BaseVariable*>(var),
      std::move(BoxedValue(result)));
  }
  return result;
}

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_EVALUATION_CONTEXT_INL_H_
