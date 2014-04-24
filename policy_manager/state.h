// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_H_

#include "update_engine/policy_manager/device_policy_provider.h"
#include "update_engine/policy_manager/config_provider.h"
#include "update_engine/policy_manager/random_provider.h"
#include "update_engine/policy_manager/shill_provider.h"
#include "update_engine/policy_manager/system_provider.h"
#include "update_engine/policy_manager/time_provider.h"
#include "update_engine/policy_manager/updater_provider.h"

namespace chromeos_policy_manager {

// The State class is an interface to the ensemble of providers. This class
// gives visibility of the state providers to policy implementations.
class State {
 public:
  virtual ~State() {}

  // These methods return the given provider.
  virtual ConfigProvider* config_provider() = 0;
  virtual DevicePolicyProvider* device_policy_provider() = 0;
  virtual RandomProvider* random_provider() = 0;
  virtual ShillProvider* shill_provider() = 0;
  virtual SystemProvider* system_provider() = 0;
  virtual TimeProvider* time_provider() = 0;
  virtual UpdaterProvider* updater_provider() = 0;

 protected:
  State() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(State);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_H_
