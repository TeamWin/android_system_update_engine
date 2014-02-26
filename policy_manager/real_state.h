// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_STATE_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_STATE_H

#include "update_engine/policy_manager/state.h"

namespace chromeos_policy_manager {

// State implementation class.
class RealState : public State {
 public:
  RealState();
  ~RealState() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RealState);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_STATE_H
