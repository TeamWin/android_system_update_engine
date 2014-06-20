// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_UPDATE_MANAGER_INL_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_UPDATE_MANAGER_INL_H_

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
  const std::string policy_name = policy_->PolicyRequestName(policy_method);
  const bool timed_out = ec->is_expired();

  // Reset the evaluation context.
  ec->ResetEvaluation();

  LOG(INFO) << "Evaluating " << policy_name << " START";

  // First try calling the actual policy, if the request did not time out.
  EvalStatus status = EvalStatus::kFailed;
  if (timed_out) {
    LOG(WARNING) << "Skipping reevaluation because the request timed out.";
  } else {
    std::string error;
    status = (policy_.get()->*policy_method)(ec, state_.get(), &error, result,
                                             args...);
    LOG_IF(WARNING, status == EvalStatus::kFailed)
        << "Evaluating policy failed: " << error;
  }

  // If evaluating the main policy failed, defer to the default policy.
  if (status == EvalStatus::kFailed) {
    std::string error;
    status = (default_policy_.*policy_method)(ec, state_.get(), &error, result,
                                              args...);
    LOG_IF(WARNING, status == EvalStatus::kFailed)
        << "Evaluating default policy failed: " << error;

    if (timed_out && status == EvalStatus::kAskMeAgainLater) {
      LOG(WARNING) << "Default policy would block but request timed out, "
                   << "forcing failure.";
      status = EvalStatus::kFailed;
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
  // Evaluate the policy.
  R result;
  EvalStatus status = EvaluatePolicy(ec, policy_method, &result, args...);

  if (status != EvalStatus::kAskMeAgainLater) {
    // AsyncPolicyRequest finished.
    callback.Run(status, result);
    return;
  }

  // Re-schedule the policy request based on used variables.
  base::Closure reeval_callback = base::Bind(
      &UpdateManager::OnPolicyReadyToEvaluate<R, Args...>,
      base::Unretained(this), ec, callback,
      policy_method, args...);
  if (ec->RunOnValueChangeOrTimeout(reeval_callback))
    return;  // Reevaluation scheduled successfully.

  // Scheduling a reevaluation can fail because policy method didn't use any
  // non-const variable nor there's any time-based event that will change the
  // status of evaluation.  Alternatively, this may indicate an error in the use
  // of the scheduling interface.
  LOG(ERROR) << "Failed to schedule a reevaluation of policy "
             << policy_->PolicyRequestName(policy_method) << "; this is a bug.";
  callback.Run(status, result);
}

template<typename R, typename... ActualArgs, typename... ExpectedArgs>
EvalStatus UpdateManager::PolicyRequest(
    EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                        std::string*, R*,
                                        ExpectedArgs...) const,
    R* result, ActualArgs... args) {
  scoped_refptr<EvaluationContext> ec(
      new EvaluationContext(clock_, evaluation_timeout_));
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
      << "Sync request used with an async policy; this is a bug";
  return ret;
}

template<typename R, typename... ActualArgs, typename... ExpectedArgs>
void UpdateManager::AsyncPolicyRequest(
    base::Callback<void(EvalStatus, const R& result)> callback,
    base::TimeDelta request_timeout,
    EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                        std::string*, R*,
                                        ExpectedArgs...) const,
    ActualArgs... args) {
  scoped_refptr<EvaluationContext> ec =
      new EvaluationContext(clock_, evaluation_timeout_, request_timeout);
  // IMPORTANT: To ensure that ActualArgs can be converted to ExpectedArgs, we
  // explicitly instantiate UpdateManager::OnPolicyReadyToEvaluate with the
  // latter in lieu of the former.
  base::Closure eval_callback = base::Bind(
      &UpdateManager::OnPolicyReadyToEvaluate<R, ExpectedArgs...>,
      base::Unretained(this), ec, callback, policy_method, args...);
  RunFromMainLoop(eval_callback);
}

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_UPDATE_MANAGER_INL_H_
