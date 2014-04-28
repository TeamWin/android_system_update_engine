// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_CHROMEOS_POLICY_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_CHROMEOS_POLICY_H_

#include "update_engine/policy_manager/policy.h"

namespace chromeos_policy_manager {

// ChromeOSPolicy implements the policy-related logic used in ChromeOS.
class ChromeOSPolicy : public Policy {
 public:
  ChromeOSPolicy() {}
  virtual ~ChromeOSPolicy() {}

  // Policy overrides.
  virtual EvalStatus UpdateCheckAllowed(EvaluationContext* ec, State* state,
                                        std::string* error,
                                        bool* result) const override;

  virtual EvalStatus UpdateDownloadAndApplyAllowed(EvaluationContext* ec,
                                                   State* state,
                                                   std::string* error,
                                                   bool* result) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeOSPolicy);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_CHROMEOS_POLICY_H_
