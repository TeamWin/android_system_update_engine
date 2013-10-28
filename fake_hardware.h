// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_HARDWARE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_HARDWARE_H__

#include "update_engine/hardware_interface.h"

#include "base/basictypes.h"

#include <map>

namespace chromeos_update_engine {

// Implements a fake hardware interface used for testing.
class FakeHardware : public HardwareInterface {
 public:
  FakeHardware() {}

  // HardwareInterface methods.
  virtual const std::string BootDevice() { return boot_device_; }

  // Setters
  void SetBootDevice(const std::string boot_device) {
    boot_device_ = boot_device;
  }

 private:
  std::string boot_device_;

  DISALLOW_COPY_AND_ASSIGN(FakeHardware);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_HARDWARE_H__
