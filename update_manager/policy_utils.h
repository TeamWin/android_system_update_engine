// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_POLICY_UTILS_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_POLICY_UTILS_H_

#include "update_engine/update_manager/policy.h"

// Checks that the passed pointer value is not null, returning kFailed on the
// current context and setting the *error description when it is null. The
// intended use is to validate variable failures while using
// EvaluationContext::GetValue, for example:
//
//   const int* my_value = ec->GetValue(state->my_provider()->var_my_value());
//   POLICY_CHECK_VALUE_AND_FAIL(my_value, error);
//
#define POLICY_CHECK_VALUE_AND_FAIL(ptr, error) \
    do { \
      if ((ptr) == nullptr) { \
        *(error) = #ptr " is required but is null."; \
        return EvalStatus::kFailed; \
      } \
    } while (false)

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_POLICY_UTILS_H_
