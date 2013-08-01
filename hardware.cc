// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/hardware.h"
#include "update_engine/utils.h"

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
  if (boot_device.empty())
    return boot_device;

  string ubiblock_prefix("/dev/ubiblock");
  string ret;
  char partition_num;
  if(utils::StringHasPrefix(boot_device, ubiblock_prefix)) {
    // eg: /dev/ubiblock3_0 becomes /dev/mtdblock2
    ret = "/dev/mtdblock";
    partition_num = boot_device[ubiblock_prefix.size()];
  } else {
    // eg: /dev/sda3 becomes /dev/sda2
    // eg: /dev/mmcblk0p3 becomes /dev/mmcblk0p2
    ret = boot_device.substr(0, boot_device.size() - 1);
    partition_num = boot_device[boot_device.size() - 1];
  }

  // Currently this assumes the partition number of the boot device is
  // 3, 5, or 7, and changes it to 2, 4, or 6, respectively, to
  // get the kernel device.
  if (partition_num == '3' || partition_num == '5' || partition_num == '7') {
    ret.append(1, partition_num - 1);
    return ret;
  }

  return "";
}

}  // namespace chromeos_update_engine
