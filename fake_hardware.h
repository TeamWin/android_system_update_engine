// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_HARDWARE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_HARDWARE_H_

#include <map>

#include <base/time/time.h>

#include "update_engine/hardware_interface.h"

namespace chromeos_update_engine {

// Implements a fake hardware interface used for testing.
class FakeHardware : public HardwareInterface {
 public:
  FakeHardware()
    : kernel_device_("/dev/sdz4"),
      boot_device_("/dev/sdz5"),
      is_boot_device_removable_(false),
      kernel_devices_({"/dev/sdz2", "/dev/sdz4"}),
      is_official_build_(true),
      is_normal_boot_mode_(true),
      is_oobe_complete_(false),
      hardware_class_("Fake HWID BLAH-1234"),
      firmware_version_("Fake Firmware v1.0.1"),
      ec_version_("Fake EC v1.0a") {}

  // HardwareInterface methods.
  virtual std::string BootKernelDevice() const override {
    return kernel_device_;
  }

  virtual std::string BootDevice() const override { return boot_device_; }

  virtual bool IsBootDeviceRemovable() const override {
    return is_boot_device_removable_;
  }

  virtual std::vector<std::string> GetKernelDevices() const override {
    return kernel_devices_;
  }

  virtual bool IsKernelBootable(const std::string& kernel_device,
                                bool* bootable) const override {
    auto i = is_bootable_.find(kernel_device);
    *bootable = (i != is_bootable_.end()) ? i->second : true;
    return true;
  }

  virtual bool MarkKernelUnbootable(const std::string& kernel_device) override {
    is_bootable_[kernel_device] = false;
    return true;
  }

  virtual bool IsOfficialBuild() const override { return is_official_build_; }

  virtual bool IsNormalBootMode() const override {
    return is_normal_boot_mode_;
  }

  virtual bool IsOOBEComplete(base::Time* out_time_of_oobe) const override {
    if (out_time_of_oobe != nullptr)
      *out_time_of_oobe = oobe_timestamp_;
    return is_oobe_complete_;
  }

  virtual std::string GetHardwareClass() const override {
    return hardware_class_;
  }

  virtual std::string GetFirmwareVersion() const override {
    return firmware_version_;
  }

  virtual std::string GetECVersion() const override { return ec_version_; }

  // Setters
  void SetBootDevice(const std::string& boot_device) {
    boot_device_ = boot_device;
  }

  void SetIsBootDeviceRemovable(bool is_boot_device_removable) {
    is_boot_device_removable_ = is_boot_device_removable;
  }

  void SetIsOfficialBuild(bool is_official_build) {
    is_official_build_ = is_official_build;
  }

  void SetIsNormalBootMode(bool is_normal_boot_mode) {
    is_normal_boot_mode_ = is_normal_boot_mode;
  }

  // Sets the IsOOBEComplete to True with the given timestamp.
  void SetIsOOBEComplete(base::Time oobe_timestamp) {
    is_oobe_complete_ = true;
    oobe_timestamp_ = oobe_timestamp;
  }

  // Sets the IsOOBEComplete to False.
  void UnsetIsOOBEComplete() {
    is_oobe_complete_ = false;
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
  std::string kernel_device_;
  std::string boot_device_;
  bool is_boot_device_removable_;
  std::vector<std::string>  kernel_devices_;
  std::map<std::string, bool> is_bootable_;
  bool is_official_build_;
  bool is_normal_boot_mode_;
  bool is_oobe_complete_;
  base::Time oobe_timestamp_;
  std::string hardware_class_;
  std::string firmware_version_;
  std::string ec_version_;

  DISALLOW_COPY_AND_ASSIGN(FakeHardware);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_HARDWARE_H_
