// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_STATE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_STATE_H_

#include "update_engine/policy_manager/fake_config_provider.h"
#include "update_engine/policy_manager/fake_device_policy_provider.h"
#include "update_engine/policy_manager/fake_random_provider.h"
#include "update_engine/policy_manager/fake_shill_provider.h"
#include "update_engine/policy_manager/fake_system_provider.h"
#include "update_engine/policy_manager/fake_time_provider.h"
#include "update_engine/policy_manager/fake_updater_provider.h"
#include "update_engine/policy_manager/state.h"

namespace chromeos_policy_manager {

// A fake State class that creates fake providers for all the providers.
class FakeState : public State {
 public:
  // Creates and initializes the FakeState using fake providers. Returns NULL
  // if the initialization fails.
  static FakeState* Construct();

  virtual ~FakeState() {}

  // Downcasted getters, to allow access to the fake instances during testing.
  virtual FakeConfigProvider* config_provider() override {
    return &config_provider_;
  }

  virtual FakeDevicePolicyProvider* device_policy_provider() override {
    return &device_policy_provider_;
  }

  virtual FakeRandomProvider* random_provider() override {
    return &random_provider_;
  }

  virtual FakeShillProvider* shill_provider() override {
    return &shill_provider_;
  }

  virtual FakeSystemProvider* system_provider() override {
    return &system_provider_;
  }

  virtual FakeTimeProvider* time_provider() override {
    return &time_provider_;
  }

  virtual FakeUpdaterProvider* updater_provider() override {
    return &updater_provider_;
  }

 private:
  FakeState() {}

  FakeConfigProvider config_provider_;
  FakeDevicePolicyProvider device_policy_provider_;
  FakeRandomProvider random_provider_;
  FakeShillProvider shill_provider_;
  FakeSystemProvider system_provider_;
  FakeTimeProvider time_provider_;
  FakeUpdaterProvider updater_provider_;

  DISALLOW_COPY_AND_ASSIGN(FakeState);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_STATE_H_
