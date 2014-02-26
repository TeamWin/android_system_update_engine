// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_STATE_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_STATE_H

#include "update_engine/policy_manager/state.h"

namespace chromeos_policy_manager {

// A fake State class that creates Fake providers for all the providers.
class FakeState : public State {
 public:
  // Initializes the State with fake providers.
  FakeState();
  virtual ~FakeState() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeState);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_STATE_H
