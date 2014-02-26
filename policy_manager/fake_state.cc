// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/fake_random_provider.h"
#include "update_engine/policy_manager/fake_shill_provider.h"
#include "update_engine/policy_manager/fake_state.h"

namespace chromeos_policy_manager {

FakeState::FakeState() {
  set_random_provider(new FakeRandomProvider());
  set_shill_provider(new FakeShillProvider());
}

}  // namespace chromeos_policy_manager
