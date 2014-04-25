// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_POLICY_MANAGER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_POLICY_MANAGER_H_

#include "update_engine/policy_manager/policy_manager.h"

#include "update_engine/policy_manager/default_policy.h"
#include "update_engine/policy_manager/fake_state.h"

namespace chromeos_policy_manager {

class FakePolicyManager : public PolicyManager {
 public:
  explicit FakePolicyManager(chromeos_update_engine::ClockInterface* clock)
      : PolicyManager(clock, new FakeState()) {
    // The FakePolicyManager uses a DefaultPolicy.
    set_policy(new DefaultPolicy());
  }

  // PolicyManager overrides.
  using PolicyManager::set_policy;

  FakeState* state() {
    return reinterpret_cast<FakeState*>(PolicyManager::state());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePolicyManager);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_POLICY_MANAGER_H_
