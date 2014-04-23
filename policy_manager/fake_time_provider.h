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

  virtual FakeVariable<base::Time>* var_curr_date() override {
    return &var_curr_date_;
  }

  virtual FakeVariable<int>* var_curr_hour() override {
    return &var_curr_hour_;
  }

 private:
  virtual bool DoInit() override { return true; }

  FakeVariable<base::Time> var_curr_date_{"curr_date", kVariableModePoll};
  FakeVariable<int> var_curr_hour_{"curr_hour", kVariableModePoll};

  DISALLOW_COPY_AND_ASSIGN(FakeTimeProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_TIME_PROVIDER_H_
