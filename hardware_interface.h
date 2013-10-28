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

  // TODO(deymo): Move other hardware-dependant functions to this interface:
  // GetECVersion, GetFirmwareVersion, GetHardwareClass, IsNormalBootMode and
  // IsOfficialBuild.

  virtual ~HardwareInterface() {}
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_HARDWARE_INTERFACE_H__
