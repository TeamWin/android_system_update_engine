// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/fake_state.h"

#include "base/memory/scoped_ptr.h"

namespace chromeos_policy_manager {

FakeState* FakeState::Construct() {
  scoped_ptr<FakeState> fake_state(new FakeState());
  if (!(fake_state->config_provider_.Init() &&
        fake_state->device_policy_provider_.Init() &&
        fake_state->random_provider_.Init() &&
        fake_state->shill_provider_.Init() &&
        fake_state->system_provider_.Init() &&
        fake_state->time_provider_.Init() &&
        fake_state->updater_provider_.Init())) {
    return NULL;
  }
  return fake_state.release();
}

}  // namespace chromeos_policy_manager
