// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/fake_state.h"

#include "base/memory/scoped_ptr.h"

namespace chromeos_policy_manager {

FakeState::FakeState() : State(new FakeDevicePolicyProvider(),
                               new FakeRandomProvider(),
                               new FakeShillProvider(),
                               new FakeSystemProvider(),
                               new FakeTimeProvider(),
                               new FakeUpdaterProvider()) {
}

FakeState* FakeState::Construct() {
  scoped_ptr<FakeState> fake_state(new FakeState());
  if (!(fake_state->device_policy_provider()->Init() &&
        fake_state->random_provider()->Init() &&
        fake_state->shill_provider()->Init() &&
        fake_state->system_provider()->Init() &&
        fake_state->time_provider()->Init() &&
        fake_state->updater_provider()->Init())) {
    return NULL;
  }
  return fake_state.release();
}

}  // namespace chromeos_policy_manager
