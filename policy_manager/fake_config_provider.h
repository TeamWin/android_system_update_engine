// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_CONFIG_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_CONFIG_PROVIDER_H_

#include "update_engine/policy_manager/config_provider.h"
#include "update_engine/policy_manager/fake_variable.h"

namespace chromeos_policy_manager {

// Fake implementation of the ConfigProvider base class.
class FakeConfigProvider : public ConfigProvider {
 public:
  FakeConfigProvider() {}

 protected:
  virtual FakeVariable<bool>* var_is_oobe_enabled() override {
    return &var_is_oobe_enabled_;
  }

 private:
  FakeVariable<bool> var_is_oobe_enabled_{
      "is_oobe_enabled", kVariableModeConst};

  DISALLOW_COPY_AND_ASSIGN(FakeConfigProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_CONFIG_PROVIDER_H_
