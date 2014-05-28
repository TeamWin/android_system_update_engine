// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_FAKE_UPDATE_MANAGER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_FAKE_UPDATE_MANAGER_H_

#include "update_engine/update_manager/update_manager.h"

#include "update_engine/update_manager/default_policy.h"
#include "update_engine/update_manager/fake_state.h"

namespace chromeos_update_manager {

class FakeUpdateManager : public UpdateManager {
 public:
  explicit FakeUpdateManager(chromeos_update_engine::ClockInterface* clock)
      : UpdateManager(clock, new FakeState()) {
    // The FakeUpdateManager uses a DefaultPolicy.
    set_policy(new DefaultPolicy());
  }

  // UpdateManager overrides.
  using UpdateManager::set_policy;

  FakeState* state() {
    return reinterpret_cast<FakeState*>(UpdateManager::state());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeUpdateManager);
};

}  // namespace chromeos_update_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_FAKE_UPDATE_MANAGER_H_
