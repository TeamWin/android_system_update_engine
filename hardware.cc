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

}  // namespace chromeos_update_engine
