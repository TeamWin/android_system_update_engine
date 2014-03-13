// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_TIME_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_TIME_PROVIDER_H_

#include "update_engine/policy_manager/fake_variable.h"
#include "update_engine/policy_manager/time_provider.h"

namespace chromeos_policy_manager {

// Fake implementation of the TimeProvider base class.
class FakeTimeProvider : public TimeProvider {
 public:
  FakeTimeProvider() {}

 protected:
  virtual bool DoInit() {
    set_var_curr_date(
        new FakeVariable<base::Time>("curr_date", kVariableModePoll));
    set_var_curr_hour(
        new FakeVariable<int>("curr_hour", kVariableModePoll));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeTimeProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_TIME_PROVIDER_H_
