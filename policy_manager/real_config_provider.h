// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_CONFIG_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_CONFIG_PROVIDER_H_

#include <string>

#include <base/memory/scoped_ptr.h>

#include "update_engine/hardware_interface.h"
#include "update_engine/policy_manager/config_provider.h"
#include "update_engine/policy_manager/generic_variables.h"

namespace chromeos_policy_manager {

// ConfigProvider concrete implementation.
class RealConfigProvider : public ConfigProvider {
 public:
  explicit RealConfigProvider(
      chromeos_update_engine::HardwareInterface* hardware)
      : hardware_(hardware) {}

  // Initializes the provider and returns whether it succeeded.
  bool Init();

  Variable<bool>* var_is_oobe_enabled() override {
    return var_is_oobe_enabled_.get();
  }

 private:
  friend class PmRealConfigProviderTest;

  // Used for testing. Sets the root prefix, which is by default "". Call this
  // method before calling Init() in order to mock out the place where the files
  // are being read from.
  void SetRootPrefix(const std::string& prefix) {
    root_prefix_ = prefix;
  }

  scoped_ptr<ConstCopyVariable<bool>> var_is_oobe_enabled_;

  chromeos_update_engine::HardwareInterface* hardware_;

  // Prefix to prepend to the file paths. Useful for testing.
  std::string root_prefix_;

  DISALLOW_COPY_AND_ASSIGN(RealConfigProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_CONFIG_PROVIDER_H_
