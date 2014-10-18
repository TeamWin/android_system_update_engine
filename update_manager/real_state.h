// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_REAL_STATE_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_REAL_STATE_H_

#include <memory>

#include "update_engine/update_manager/state.h"

namespace chromeos_update_manager {

// State concrete implementation.
class RealState : public State {
 public:
  virtual ~RealState() {}

  RealState(ConfigProvider* config_provider,
            DevicePolicyProvider* device_policy_provider,
            RandomProvider* random_provider,
            ShillProvider* shill_provider,
            SystemProvider* system_provider,
            TimeProvider* time_provider,
            UpdaterProvider* updater_provider) :
      config_provider_(config_provider),
      device_policy_provider_(device_policy_provider),
      random_provider_(random_provider),
      shill_provider_(shill_provider),
      system_provider_(system_provider),
      time_provider_(time_provider),
      updater_provider_(updater_provider) {}

  // These methods return the given provider.
  ConfigProvider* config_provider() override {
    return config_provider_.get();
  }
  DevicePolicyProvider* device_policy_provider() override {
    return device_policy_provider_.get();
  }
  RandomProvider* random_provider() override {
    return random_provider_.get();
  }
  ShillProvider* shill_provider() override {
    return shill_provider_.get();
  }
  SystemProvider* system_provider() override {
    return system_provider_.get();
  }
  TimeProvider* time_provider() override {
    return time_provider_.get();
  }
  UpdaterProvider* updater_provider() override {
    return updater_provider_.get();
  }

 private:
  // Instances of the providers.
  std::unique_ptr<ConfigProvider> config_provider_;
  std::unique_ptr<DevicePolicyProvider> device_policy_provider_;
  std::unique_ptr<RandomProvider> random_provider_;
  std::unique_ptr<ShillProvider> shill_provider_;
  std::unique_ptr<SystemProvider> system_provider_;
  std::unique_ptr<TimeProvider> time_provider_;
  std::unique_ptr<UpdaterProvider> updater_provider_;
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_REAL_STATE_H_
