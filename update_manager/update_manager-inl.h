// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_UPDATE_MANAGER_INL_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_UPDATE_MANAGER_INL_H_

#include <string>

#include <base/bind.h>

#include "update_engine/update_manager/evaluation_context.h"
#include "update_engine/update_manager/event_loop.h"

namespace chromeos_update_manager {

template<typename R, typename... Args>
EvalStatus UpdateManager::EvaluatePolicy(
    EvaluationContext* ec,
    EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                        std::string*, R*,
                                        Args...) const,
    R* result, Args... args) {
  std::string error;
  const std::string policy_name = policy_->PolicyRequestName(policy_method);

  // First try calling the actual policy.
  LOG(INFO) << "Evaluating " << policy_name << " START";
  EvalStatus status = (policy_.get()->*policy_method)(ec, state_.get(), &error,
                                                      result, args...);

  if (status == EvalStatus::kFailed) {
    LOG(WARNING) << "Policy request failed: " << error;
    error.clear();
    status = (default_policy_.*policy_method)(ec, state_.get(), &error,
                                              result, args...);

    if (status == EvalStatus::kFailed) {
      LOG(WARNING) << "Request to default policy also failed: " << error;
    }
  }
  LOG(INFO) << "Evaluating " << policy_name << " END";

  // TODO(deymo): Log the actual state used from the EvaluationContext.
  return status;
}

template<typename R, typename... Args>
void UpdateManager::OnPolicyReadyToEvaluate(
    scoped_refptr<EvaluationContext> ec,
    base::Callback<void(EvalStatus status, const R& result)> callback,
    EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                        std::string*, R*,
                                        Args...) const,
    Args... args) {
  ec->ResetEvaluation();
  R result;
  EvalStatus status = EvaluatePolicy(ec, policy_method, &result, args...);

  if (status != EvalStatus::kAskMeAgainLater) {
    // AsyncPolicyRequest finished.
    callback.Run(status, result);
    return;
  }
  // Re-schedule the policy request based on used variables.
  base::Closure closure = base::Bind(
      &UpdateManager::OnPolicyReadyToEvaluate<R, Args...>,
      base::Unretained(this), ec, callback, policy_method, args...);

  if (!ec->RunOnValueChangeOrTimeout(closure)) {
    // The policy method didn't use any non-const variable nor there's any
    // time-based event that will change the status of evaluation. We call the
    // callback with EvalStatus::kAskMeAgainLater.
    LOG(ERROR) << "Policy implementation didn't use any non-const variable "
                  "but returned kAskMeAgainLater.";
    callback.Run(EvalStatus::kAskMeAgainLater, result);
    return;
  }
}

template<typename R, typename... ActualArgs, typename... ExpectedArgs>
EvalStatus UpdateManager::PolicyRequest(
    EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                        std::string*, R*,
                                        ExpectedArgs...) const,
    R* result, ActualArgs... args) {
  scoped_refptr<EvaluationContext> ec(new EvaluationContext(clock_));
  // A PolicyRequest allways consists on a single evaluation on a new
  // EvaluationContext.
  // IMPORTANT: To ensure that ActualArgs can be converted to ExpectedArgs, we
  // explicitly instantiate EvaluatePolicy with the latter in lieu of the
  // former.
  EvalStatus ret = EvaluatePolicy<R, ExpectedArgs...>(ec, policy_method, result,
                                                      args...);
  // Sync policy requests must not block, if they do then this is an error.
  DCHECK(EvalStatus::kAskMeAgainLater != ret);
  LOG_IF(WARNING, EvalStatus::kAskMeAgainLater == ret)
      << "Sync request used with an async policy";
  return ret;
}

template<typename R, typename... ActualArgs, typename... ExpectedArgs>
void UpdateManager::AsyncPolicyRequest(
    base::Callback<void(EvalStatus, const R& result)> callback,
    EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                        std::string*, R*,
                                        ExpectedArgs...) const,
    ActualArgs... args) {
  scoped_refptr<EvaluationContext> ec = new EvaluationContext(clock_);
  // IMPORTANT: To ensure that ActualArgs can be converted to ExpectedArgs, we
  // explicitly instantiate UpdateManager::OnPolicyReadyToEvaluate with the
  // latter in lieu of the former.
  base::Closure closure = base::Bind(
      &UpdateManager::OnPolicyReadyToEvaluate<R, ExpectedArgs...>,
      base::Unretained(this), ec, callback, policy_method, args...);
  RunFromMainLoop(closure);
}

}  // namespace chromeos_update_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_UPDATE_MANAGER_INL_H_
