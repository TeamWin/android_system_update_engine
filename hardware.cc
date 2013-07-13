// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/hardware.h"

#include <base/logging.h>
#include <rootdev/rootdev.h>

using std::string;

namespace chromeos_update_engine {

const string Hardware::BootDevice() {
  char boot_path[PATH_MAX];
  // Resolve the boot device path fully, including dereferencing
  // through dm-verity.
  int ret = rootdev(boot_path, sizeof(boot_path), true, false);

  if (ret < 0) {
    LOG(ERROR) << "rootdev failed to find the root device";
    return "";
  }
  LOG_IF(WARNING, ret > 0) << "rootdev found a device name with no device node";

  // This local variable is used to construct the return string and is not
  // passed around after use.
  return boot_path;
}

const string Hardware::KernelDeviceOfBootDevice(
    const std::string& boot_device) {
  // Currently this assumes the last digit of the boot device is
  // 3, 5, or 7, and changes it to 2, 4, or 6, respectively, to
  // get the kernel device.
  string ret = boot_device;
  if (ret.empty())
    return ret;
  char last_char = ret[ret.size() - 1];
  if (last_char == '3' || last_char == '5' || last_char == '7') {
    ret[ret.size() - 1] = last_char - 1;
    return ret;
  }
  return "";
}

}  // namespace chromeos_update_engine
