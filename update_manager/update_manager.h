// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_UPDATE_MANAGER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_UPDATE_MANAGER_H_

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>

#include "update_engine/clock_interface.h"
#include "update_engine/update_manager/default_policy.h"
#include "update_engine/update_manager/policy.h"
#include "update_engine/update_manager/state.h"

namespace chromeos_update_manager {

// The main Update Manager singleton class.
class UpdateManager {
 public:
  // Creates the UpdateManager instance, assuming ownership on the provided
  // |state|.
  UpdateManager(chromeos_update_engine::ClockInterface* clock,
                base::TimeDelta evaluation_timeout, State* state);

  virtual ~UpdateManager() {}

  // PolicyRequest() evaluates the given policy with the provided arguments and
  // returns the result. The |policy_method| is the pointer-to-method of the
  // Policy class for the policy request to call. The UpdateManager will call
  // this method on the right policy. The pointer |result| must not be NULL and
  // the remaining |args| depend on the arguments required by the passed
  // |policy_method|.
  //
  // When the policy request succeeds, the |result| is set and the method
  // returns EvalStatus::kSucceeded, otherwise, the |result| may not be set.  A
  // policy called with this method should not block (i.e. return
  // EvalStatus::kAskMeAgainLater), which is considered a programming error. On
  // failure, EvalStatus::kFailed is returned.
  //
  // An example call to this method is:
  //   um.PolicyRequest(&Policy::SomePolicyMethod, &bool_result, arg1, arg2);
  template<typename R, typename... ActualArgs, typename... ExpectedArgs>
  EvalStatus PolicyRequest(
      EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                          std::string*, R*,
                                          ExpectedArgs...) const,
      R* result, ActualArgs...);

  // Evaluates the given |policy_method| policy with the provided |args|
  // arguments and calls the |callback| callback with the result when done.
  //
  // If the policy implementation should block, returning a
  // EvalStatus::kAskMeAgainLater status the Update Manager will re-evaluate the
  // policy until another status is returned. If the policy implementation based
  // its return value solely on const variables, the callback will be called
  // with the EvalStatus::kAskMeAgainLater status.
  template<typename R, typename... ActualArgs, typename... ExpectedArgs>
  void AsyncPolicyRequest(
      base::Callback<void(EvalStatus, const R& result)> callback,
      EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                          std::string*, R*,
                                          ExpectedArgs...) const,
      ActualArgs... args);

 protected:
  // The UpdateManager receives ownership of the passed Policy instance.
  void set_policy(const Policy* policy) {
    policy_.reset(policy);
  }

  // State getter used for testing.
  State* state() { return state_.get(); }

 private:
  FRIEND_TEST(UmUpdateManagerTest, PolicyRequestCallsPolicy);
  FRIEND_TEST(UmUpdateManagerTest, PolicyRequestCallsDefaultOnError);
  FRIEND_TEST(UmUpdateManagerTest, PolicyRequestDoesntBlockDeathTest);
  FRIEND_TEST(UmUpdateManagerTest, AsyncPolicyRequestDelaysEvaluation);

  // EvaluatePolicy() evaluates the passed |policy_method| method on the current
  // policy with the given |args| arguments. If the method fails, the default
  // policy is used instead.
  template<typename R, typename... Args>
  EvalStatus EvaluatePolicy(
      EvaluationContext* ec,
      EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                          std::string*, R*,
                                          Args...) const,
      R* result, Args... args);

  // OnPolicyReadyToEvaluate() is called by the main loop when the evaluation
  // of the given |policy_method| should be executed. If the evaluation finishes
  // the |callback| callback is called passing the |result| and the |status|
  // returned by the policy. If the evaluation returns an
  // EvalStatus::kAskMeAgainLater state, the |callback| will NOT be called and
  // the evaluation will be re-scheduled to be called later.
  template<typename R, typename... Args>
  void OnPolicyReadyToEvaluate(
      scoped_refptr<EvaluationContext> ec,
      base::Callback<void(EvalStatus status, const R& result)> callback,
      EvalStatus (Policy::*policy_method)(EvaluationContext*, State*,
                                          std::string*, R*,
                                          Args...) const,
      Args... args);

  // The policy used by the UpdateManager. Note that since it is a const Policy,
  // policy implementations are not allowed to persist state on this class.
  scoped_ptr<const Policy> policy_;

  // A safe default value to the current policy. This policy is used whenever
  // a policy implementation fails with EvalStatus::kFailed.
  const DefaultPolicy default_policy_;

  // State Providers.
  scoped_ptr<State> state_;

  // Pointer to the mockable clock interface;
  chromeos_update_engine::ClockInterface* clock_;

  // Timeout for a policy evaluation.
  const base::TimeDelta evaluation_timeout_;

  DISALLOW_COPY_AND_ASSIGN(UpdateManager);
};

}  // namespace chromeos_update_manager

// Include the implementation of the template methods.
#include "update_engine/update_manager/update_manager-inl.h"

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_UPDATE_MANAGER_H_
