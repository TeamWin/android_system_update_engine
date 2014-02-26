// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_MANAGER_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_MANAGER_H

#include <base/memory/scoped_ptr.h>

#include "update_engine/policy_manager/default_policy.h"
#include "update_engine/policy_manager/policy.h"
#include "update_engine/policy_manager/state.h"

namespace chromeos_policy_manager {

// The main Policy Manager singleton class.
class PolicyManager {
 public:
  PolicyManager() {}

  // Initializes the PolicyManager instance. Returns whether the initialization
  // succeeded.
  bool Init();

  // PolicyRequest() evaluates the given policy with the provided arguments and
  // returns the result. The |policy_method| is the pointer-to-method of the
  // Policy class for the policy request to call. The PolicyManager will call
  // this method on the right policy. The pointer |result| must not be NULL and
  // the remaining |args| depend on the arguments required by the passed
  // |policy_method|.
  //
  // When the policy request succeeds, the |result| is set and the method
  // returns EvalStatusSucceeded, otherwise, the |result| may not be set. Also,
  // if the policy implementation should block, this method returns immediately
  // with EvalStatusAskMeAgainLater. In case of failure EvalStatusFailed is
  // returned and the |error| message is set, which must not be NULL.
  //
  // An example call to this method is:
  //   pm.PolicyRequest(&Policy::SomePolicyMethod, &bool_result, arg1, arg2);
  template<typename T, typename R, typename... Args>
  EvalStatus PolicyRequest(T policy_method, R* result, Args... args);

 private:
  friend class PmPolicyManagerTest;
  FRIEND_TEST(PmPolicyManagerTest, PolicyRequestCallsPolicy);
  FRIEND_TEST(PmPolicyManagerTest, PolicyRequestCallsDefaultOnError);
  FRIEND_TEST(PmPolicyManagerTest, PolicyRequestDoesntBlock);

  // The policy used by the PolicyManager. Note that since it is a const Policy,
  // policy implementations are not allowed to persist state on this class.
  scoped_ptr<const Policy> policy_;

  // A safe default value to the current policy. This policy is used whenever
  // a policy implementation fails with EvalStatusFailed.
  const DefaultPolicy default_policy_;

  // State Providers.
  scoped_ptr<State> state_;

  DISALLOW_COPY_AND_ASSIGN(PolicyManager);
};

}  // namespace chromeos_policy_manager

// Include the implementation of the template methods.
#include "update_engine/policy_manager/policy_manager-inl.h"

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_POLICY_MANAGER_H
