// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/fake_hardware.h"

using std::string;

namespace chromeos_update_engine {

const string FakeHardware::KernelDeviceOfBootDevice(
    const string& boot_device) {
  KernelDevicesMap::iterator it = kernel_devices_.find(boot_device);
  if (it == kernel_devices_.end())
    return "";
  return it->second;
}

void FakeHardware::SetKernelDeviceOfBootDevice(const string& boot_device,
                                               const string& kernel_device) {
  kernel_devices_[boot_device] = kernel_device;
}

}  // namespace chromeos_update_engine
