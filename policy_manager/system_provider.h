// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_SYSTEM_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_SYSTEM_PROVIDER_H_

#include <base/memory/scoped_ptr.h>

#include "update_engine/policy_manager/provider.h"
#include "update_engine/policy_manager/variable.h"

namespace chromeos_policy_manager {

// Provider for system information, mostly constant, such as the information
// reported by crossystem, the kernel boot command line and the partition table.
class SystemProvider : public Provider {
 public:
  // Returns true if the boot mode is normal or if it's unable to
  // determine the boot mode. Returns false if the boot mode is
  // developer.
  Variable<bool>* var_is_normal_boot_mode() const {
    return var_is_normal_boot_mode_.get();
  }

  // Returns whether this is an official Chrome OS build.
  Variable<bool>* var_is_official_build() const {
    return var_is_official_build_.get();
  }

 protected:
  SystemProvider() {}

  void set_var_is_normal_boot_mode(Variable<bool>* var_is_normal_boot_mode) {
    var_is_normal_boot_mode_.reset(var_is_normal_boot_mode);
  }

  void set_var_is_official_build(Variable<bool>* var_is_official_build) {
    var_is_official_build_.reset(var_is_official_build);
  }

 private:
  scoped_ptr<Variable<bool>> var_is_normal_boot_mode_;
  scoped_ptr<Variable<bool>> var_is_official_build_;

  DISALLOW_COPY_AND_ASSIGN(SystemProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_SYSTEM_PROVIDER_H_
