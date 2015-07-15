// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_VERITY_UTILS_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_VERITY_UTILS_H_

#include <string>

namespace chromeos_update_engine {

bool GetVerityRootfsSize(const std::string& kernel_dev, uint64_t* rootfs_size);

bool ParseVerityRootfsSize(const std::string& kernel_cmdline,
                           uint64_t* rootfs_size);

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_VERITY_UTILS_H_
