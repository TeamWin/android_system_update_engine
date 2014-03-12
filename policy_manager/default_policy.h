// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_DEFAULT_POLICY_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_DEFAULT_POLICY_H

#include "update_engine/policy_manager/policy.h"

namespace chromeos_policy_manager {

// The DefaultPolicy is a safe Policy implementation that doesn't fail. The
// values returned by this policy are safe default in case of failure of the
// actual policy being used by the PolicyManager.
class DefaultPolicy : public Policy {
 public:
  DefaultPolicy() {}
  virtual ~DefaultPolicy() {}

  // Policy overrides.
  virtual EvalStatus UpdateCheckAllowed(EvaluationContext* ec, State* state,
                                        std::string* error,
                                        bool* result) const {
    *result = true;
    return EvalStatus::kSucceeded;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPolicy);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_DEFAULT_POLICY_H
