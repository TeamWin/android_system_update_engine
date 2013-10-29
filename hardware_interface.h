// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_HARDWARE_INTERFACE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_HARDWARE_INTERFACE_H__

#include <string>

namespace chromeos_update_engine {

// The hardware interface allows access to the following parts of the system,
// closely related to the hardware:
//  * crossystem exposed properties: firmware, hwid, etc.
//  * Physical disk: partition booted from and partition name conversions.
// These stateless functions are tied together in this interface to facilitate
// unit testing.
class HardwareInterface {
 public:
  // Returns the currently booted device. "/dev/sda3", for example.
  // This will not interpret LABEL= or UUID=. You'll need to use findfs
  // or something with equivalent funcionality to interpret those.
  virtual const std::string BootDevice() = 0;

  // Returns true if this is an official Chrome OS build, false otherwise.
  virtual bool IsOfficialBuild() = 0;

  // Returns true if the boot mode is normal or if it's unable to
  // determine the boot mode. Returns false if the boot mode is
  // developer.
  virtual bool IsNormalBootMode() = 0;

  // Returns the HWID or an empty string on error.
  virtual std::string GetHardwareClass() = 0;

  // Returns the firmware version or an empty string if the system is
  // not running chrome os firmware.
  virtual std::string GetFirmwareVersion() = 0;

  // Returns the ec version or an empty string if the system is not
  // running a custom chrome os ec.
  virtual std::string GetECVersion() = 0;

  virtual ~HardwareInterface() {}
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_HARDWARE_INTERFACE_H__
