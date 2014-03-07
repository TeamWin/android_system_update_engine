// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_EVALUATION_CONTEXT_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_EVALUATION_CONTEXT_H_

#include <map>

#include <base/memory/ref_counted.h>

#include "update_engine/policy_manager/variable.h"
#include "update_engine/policy_manager/boxed_value.h"

namespace chromeos_policy_manager {

// The EvaluationContext class is the interface between a policy implementation
// and the state. The EvaluationContext tracks the variables used by a policy
// request and caches the returned values, owning those cached values.
class EvaluationContext : public base::RefCounted<EvaluationContext> {
 public:
  EvaluationContext() {}

  // Returns a pointer to the value returned by the passed variable |var|. The
  // EvaluationContext instance keeps the ownership of the returned object. The
  // returned object is valid during the life of the EvaluationContext, even if
  // the passed Variable changes it.
  //
  // In case of error, a NULL value is returned.
  template<typename T>
  const T* GetValue(Variable<T>* var);

 private:
  // The remaining time for the current evaluation.
  base::TimeDelta RemainingTime() const;

  // A map to hold the cached values for every variable.
  typedef std::map<BaseVariable*, BoxedValue> ValueCacheMap;

  // The cached values of the called Variables.
  ValueCacheMap value_cache_;

  DISALLOW_COPY_AND_ASSIGN(EvaluationContext);
};

}  // namespace chromeos_policy_manager

// Include the implementation of the template methods.
#include "update_engine/policy_manager/evaluation_context-inl.h"

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_EVALUATION_CONTEXT_H_
