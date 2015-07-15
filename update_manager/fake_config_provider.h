// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_FAKE_CONFIG_PROVIDER_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_FAKE_CONFIG_PROVIDER_H_

#include "update_engine/update_manager/config_provider.h"
#include "update_engine/update_manager/fake_variable.h"

namespace chromeos_update_manager {

// Fake implementation of the ConfigProvider base class.
class FakeConfigProvider : public ConfigProvider {
 public:
  FakeConfigProvider() {}

  FakeVariable<bool>* var_is_oobe_enabled() override {
    return &var_is_oobe_enabled_;
  }

 private:
  FakeVariable<bool> var_is_oobe_enabled_{  // NOLINT(whitespace/braces)
      "is_oobe_enabled", kVariableModeConst};

  DISALLOW_COPY_AND_ASSIGN(FakeConfigProvider);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_FAKE_CONFIG_PROVIDER_H_
