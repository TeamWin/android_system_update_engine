// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_FAKE_SYSTEM_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_FAKE_SYSTEM_PROVIDER_H_

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

 private:
  FakeVariable<bool> var_is_normal_boot_mode_{
      "is_normal_boot_mode", kVariableModeConst};
  FakeVariable<bool> var_is_official_build_{
      "is_official_build", kVariableModeConst};
  FakeVariable<bool> var_is_oobe_complete_{
      "is_oobe_complete", kVariableModePoll};

  DISALLOW_COPY_AND_ASSIGN(FakeSystemProvider);
};

}  // namespace chromeos_update_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_FAKE_SYSTEM_PROVIDER_H_
