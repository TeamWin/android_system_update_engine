// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SYSTEM_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SYSTEM_PROVIDER_H_

#include <string>

#include "update_engine/policy_manager/system_provider.h"

namespace chromeos_policy_manager {

// SystemProvider concrete implementation.
class RealSystemProvider : public SystemProvider {
 public:
  RealSystemProvider() {}

  // Initializes the provider and returns whether it succeeded.
  bool Init();

  virtual Variable<bool>* var_is_normal_boot_mode() override {
    return var_is_normal_boot_mode_.get();
  }

  virtual Variable<bool>* var_is_official_build() override {
    return var_is_official_build_.get();
  }

 private:
  scoped_ptr<Variable<bool>> var_is_normal_boot_mode_;
  scoped_ptr<Variable<bool>> var_is_official_build_;

  DISALLOW_COPY_AND_ASSIGN(RealSystemProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SYSTEM_PROVIDER_H_
