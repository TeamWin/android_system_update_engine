// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_state.h"

#include "update_engine/policy_manager/real_random_provider.h"
#include "update_engine/policy_manager/real_shill_provider.h"
#include "update_engine/policy_manager/real_time_provider.h"

namespace chromeos_policy_manager {

RealState::RealState(RandomProvider* random_provider,
                     ShillProvider* shill_provider,
                     TimeProvider* time_provider) {
  set_random_provider(random_provider);
  set_shill_provider(shill_provider);
  set_time_provider(time_provider);
}

}  // namespace chromeos_policy_manager
