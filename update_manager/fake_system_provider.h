// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_FAKE_SYSTEM_PROVIDER_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_FAKE_SYSTEM_PROVIDER_H_

#include "update_engine/update_manager/fake_variable.h"
#include "update_engine/update_manager/system_provider.h"

namespace chromeos_update_manager {

// Fake implementation of the SystemProvider base class.
class FakeSystemProvider : public SystemProvider {
 public:
  FakeSystemProvider() {}

  virtual FakeVariable<bool>* var_is_normal_boot_mode() override {
    return &var_is_normal_boot_mode_;
  }

  virtual FakeVariable<bool>* var_is_official_build() override {
    return &var_is_official_build_;
  }

  virtual FakeVariable<bool>* var_is_oobe_complete() override {
    return &var_is_oobe_complete_;
  }

  virtual FakeVariable<bool>* var_is_boot_device_removable() override {
    return &var_is_boot_device_removable_;
  }

 private:
  FakeVariable<bool> var_is_normal_boot_mode_{  // NOLINT(whitespace/braces)
    "is_normal_boot_mode", kVariableModeConst};
  FakeVariable<bool> var_is_official_build_{  // NOLINT(whitespace/braces)
    "is_official_build", kVariableModeConst};
  FakeVariable<bool> var_is_oobe_complete_{  // NOLINT(whitespace/braces)
    "is_oobe_complete", kVariableModePoll};
  FakeVariable<bool>
      var_is_boot_device_removable_{  // NOLINT(whitespace/braces)
        "is_boot_device_removable", kVariableModePoll};

  DISALLOW_COPY_AND_ASSIGN(FakeSystemProvider);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_FAKE_SYSTEM_PROVIDER_H_
