// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_MANAGER_INL_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_MANAGER_INL_H

#include <base/bind.h>

#include "update_engine/policy_manager/evaluation_context.h"

namespace chromeos_policy_manager {

template<typename T, typename R, typename... Args>
EvalStatus PolicyManager::EvaluatePolicy(EvaluationContext* ec,
                                         T policy_method, R* result,
                                         Args... args) {
  std::string error;

  // First try calling the actual policy.
  EvalStatus status = (policy_.get()->*policy_method)(ec, state_.get(), &error,
                                                      result, args...);

  if (status == EvalStatus::kFailed) {
    LOG(WARNING) << "PolicyRequest() failed with error: " << error;
    error.clear();
    status = (default_policy_.*policy_method)(ec, state_.get(), &error,
                                              result, args...);

    if (status == EvalStatus::kFailed) {
      LOG(WARNING) << "Request to DefaultPolicy also failed, passing error.";
    }
  }
  // TODO(deymo): Log the actual state used from the EvaluationContext.
  return status;
}


template<typename T, typename R, typename... Args>
void PolicyManager::OnPolicyReadyToEvaluate(
    scoped_refptr<EvaluationContext> ec,
    base::Callback<void(EvalStatus status, const R& result)> callback,
    T policy_method, Args... args) {
  R result;
  EvalStatus status = EvaluatePolicy(ec, policy_method, &result, args...);

  if (status != EvalStatus::kAskMeAgainLater) {
    // AsyncPolicyRequest finished.
    callback.Run(status, result);
    return;
  }
  // Re-schedule the policy request.
  // TODO(deymo): Use the information gathered on the EvaluationContext to
  // hook on proper events from changes on the State in order to re-evaluate
  // the policy after some period of time.
  base::Closure closure = base::Bind(
      &PolicyManager::OnPolicyReadyToEvaluate<T, R, Args...>,
      base::Unretained(this), ec, callback, policy_method, args...);
  RunFromMainLoopAfterTimeout(closure, base::TimeDelta::FromSeconds(20));
}

template<typename T, typename R, typename... Args>
EvalStatus PolicyManager::PolicyRequest(T policy_method, R* result,
                                        Args... args) {
  scoped_refptr<EvaluationContext> ec(new EvaluationContext);
  // A PolicyRequest allways consists on a single evaluation on a new
  // EvaluationContext.
  return EvaluatePolicy(ec, policy_method, result, args...);
}

template<typename T, typename R, typename... Args>
void PolicyManager::AsyncPolicyRequest(
    base::Callback<void(EvalStatus, const R& result)> callback,
    T policy_method, Args... args) {

  scoped_refptr<EvaluationContext> ec = new EvaluationContext;
  base::Closure closure = base::Bind(
      &PolicyManager::OnPolicyReadyToEvaluate<T, R, Args...>,
      base::Unretained(this), ec, callback, policy_method, args...);
  RunFromMainLoop(closure);
}

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_MANAGER_INL_H
