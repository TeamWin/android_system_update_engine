// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_SYSTEM_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_SYSTEM_PROVIDER_H_

#include "update_engine/policy_manager/fake_variable.h"
#include "update_engine/policy_manager/system_provider.h"

namespace chromeos_policy_manager {

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

 private:
  virtual bool DoInit() override { return true; }

  FakeVariable<bool> var_is_normal_boot_mode_{
      "is_normal_boot_mode", kVariableModeConst};
  FakeVariable<bool> var_is_official_build_{
      "is_official_build", kVariableModeConst};

  DISALLOW_COPY_AND_ASSIGN(FakeSystemProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_SYSTEM_PROVIDER_H_
