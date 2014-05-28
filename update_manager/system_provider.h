// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_SYSTEM_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_SYSTEM_PROVIDER_H_

#include "update_engine/update_manager/provider.h"
#include "update_engine/update_manager/variable.h"

namespace chromeos_update_manager {

// Provider for system information, mostly constant, such as the information
// reported by crossystem, the kernel boot command line and the partition table.
class SystemProvider : public Provider {
 public:
  virtual ~SystemProvider() {}

  // Returns true if the boot mode is normal or if it's unable to
  // determine the boot mode. Returns false if the boot mode is
  // developer.
  virtual Variable<bool>* var_is_normal_boot_mode() = 0;

  // Returns whether this is an official Chrome OS build.
  virtual Variable<bool>* var_is_official_build() = 0;

  // Returns a variable that tells whether OOBE was completed.
  virtual Variable<bool>* var_is_oobe_complete() = 0;

 protected:
  SystemProvider() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemProvider);
};

}  // namespace chromeos_update_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_SYSTEM_PROVIDER_H_
