// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_system_provider.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <vboot/crossystem.h>

#include "update_engine/policy_manager/generic_variables.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_policy_manager {

bool RealSystemProvider::Init() {
  var_is_normal_boot_mode_.reset(
      new ConstCopyVariable<bool>("is_normal_boot_mode",
                                  VbGetSystemPropertyInt("devsw_boot") != 0));

  var_is_official_build_.reset(
      new ConstCopyVariable<bool>("var_is_official_build",
                                  VbGetSystemPropertyInt("debug_build") == 0));

  return true;
}

}  // namespace chromeos_policy_manager
