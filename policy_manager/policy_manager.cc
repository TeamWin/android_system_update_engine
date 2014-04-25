// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/policy_manager.h"

#include "update_engine/policy_manager/chromeos_policy.h"
#include "update_engine/policy_manager/state.h"

using base::Closure;

namespace chromeos_policy_manager {

PolicyManager::PolicyManager(chromeos_update_engine::ClockInterface* clock,
                             State* state)
      : state_(state), clock_(clock) {
  // TODO(deymo): Make it possible to replace this policy with a different
  // implementation with a build-time flag.
  policy_.reset(new ChromeOSPolicy());
}

}  // namespace chromeos_policy_manager
