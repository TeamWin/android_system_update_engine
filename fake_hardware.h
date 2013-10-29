// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_HARDWARE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_HARDWARE_H__

#include "update_engine/hardware_interface.h"

namespace chromeos_update_engine {

// Implements a fake hardware interface used for testing.
class FakeHardware : public HardwareInterface {
 public:
  FakeHardware()
    : boot_device_("/dev/sdz5"),
      hardware_class_("Fake HWID BLAH-1234"),
      firmware_version_("Fake Firmware v1.0.1"),
      ec_version_("Fake EC v1.0a") {}

  // HardwareInterface methods.
  virtual const std::string BootDevice() { return boot_device_; }
  virtual std::string GetHardwareClass() { return hardware_class_; }
  virtual std::string GetFirmwareVersion() { return firmware_version_; }
  virtual std::string GetECVersion() { return ec_version_; }

  // Setters
  void SetBootDevice(const std::string boot_device) {
    boot_device_ = boot_device;
  }

  void SetHardwareClass(std::string hardware_class) {
    hardware_class_ = hardware_class;
  }

  void SetFirmwareVersion(std::string firmware_version) {
    firmware_version_ = firmware_version;
  }

  void SetECVersion(std::string ec_version) {
    ec_version_ = ec_version;
  }

 private:
  std::string boot_device_;
  std::string hardware_class_;
  std::string firmware_version_;
  std::string ec_version_;

  DISALLOW_COPY_AND_ASSIGN(FakeHardware);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_HARDWARE_H__
