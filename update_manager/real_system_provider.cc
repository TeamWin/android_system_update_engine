// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/real_system_provider.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <vboot/crossystem.h>

#include "update_engine/update_manager/generic_variables.h"
#include "update_engine/utils.h"

using base::StringPrintf;
using base::Time;
using base::TimeDelta;
using std::string;
using std::vector;

namespace chromeos_update_manager {

bool RealSystemProvider::Init() {
  var_is_normal_boot_mode_.reset(
      new ConstCopyVariable<bool>("is_normal_boot_mode",
                                  VbGetSystemPropertyInt("devsw_boot") != 0));

  var_is_official_build_.reset(
      new ConstCopyVariable<bool>("is_official_build",
                                  VbGetSystemPropertyInt("debug_build") == 0));

  var_is_oobe_complete_.reset(
      new CallCopyVariable<bool>(
          "is_oobe_complete",
          base::Bind(&chromeos_update_engine::HardwareInterface::IsOOBEComplete,
                     base::Unretained(hardware_), nullptr)));

  return true;
}

}  // namespace chromeos_update_manager
