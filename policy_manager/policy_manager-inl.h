// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_MANAGER_INL_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_MANAGER_INL_H

#include "update_engine/policy_manager/evaluation_context.h"

namespace chromeos_policy_manager {

// Provider of random values.
template<typename T, typename R, typename... Args>
EvalStatus PolicyManager::PolicyRequest(T policy_method, R* result,
                                        Args... args) {
  EvaluationContext ec;
  std::string error;

  // First try calling the actual policy.
  EvalStatus status = (policy_.get()->*policy_method)(&ec, state_.get(), &error,
                                                      result, args...);

  if (status == EvalStatus::kFailed) {
    LOG(WARNING) << "PolicyRequest() failed with error: " << error;
    error.clear();
    status = (default_policy_.*policy_method)(&ec, state_.get(), &error,
                                              result, args...);

    if (status == EvalStatus::kFailed) {
      LOG(WARNING) << "Request to DefaultPolicy also failed, passing error.";
    }
  }
  // TODO(deymo): Log the actual state used from the EvaluationContext.
  return status;
}

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_MANAGER_INL_H
