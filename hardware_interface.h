//
// Copyright (C) 2013 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef UPDATE_ENGINE_HARDWARE_INTERFACE_H_
#define UPDATE_ENGINE_HARDWARE_INTERFACE_H_

#include <string>
#include <vector>

#include <base/time/time.h>

namespace chromeos_update_engine {

// The hardware interface allows access to the following parts of the system,
// closely related to the hardware:
//  * crossystem exposed properties: firmware, hwid, etc.
//  * Physical disk: partition booted from and partition name conversions.
// These stateless functions are tied together in this interface to facilitate
// unit testing.
class HardwareInterface {
 public:
  virtual ~HardwareInterface() {}

  // Returns the currently booted kernel partition. "/dev/sda2", for example.
  virtual std::string BootKernelDevice() const = 0;

  // Returns the currently booted rootfs partition. "/dev/sda3", for example.
  virtual std::string BootDevice() const = 0;

  // Return whether the BootDevice() is a removable device.
  virtual bool IsBootDeviceRemovable() const = 0;

  // Returns a list of all kernel partitions available (whether bootable or not)
  virtual std::vector<std::string> GetKernelDevices() const = 0;

  // Is the specified kernel partition currently bootable, based on GPT flags?
  // Returns success.
  virtual bool IsKernelBootable(const std::string& kernel_device,
                                bool* bootable) const = 0;

  // Mark the specified kernel partition unbootable in GPT flags. We mark
  // the other kernel as bootable inside postinst, not inside the UE.
  // Returns success.
  virtual bool MarkKernelUnbootable(const std::string& kernel_device) = 0;

  // Returns true if this is an official Chrome OS build, false otherwise.
  virtual bool IsOfficialBuild() const = 0;

  // Returns true if the boot mode is normal or if it's unable to
  // determine the boot mode. Returns false if the boot mode is
  // developer.
  virtual bool IsNormalBootMode() const = 0;

  // Returns true if the OOBE process has been completed and EULA accepted,
  // False otherwise. If True is returned, and |out_time_of_oobe| isn't null,
  // the time-stamp of when OOBE happened is stored at |out_time_of_oobe|.
  virtual bool IsOOBEComplete(base::Time* out_time_of_oobe) const = 0;

  // Returns the HWID or an empty string on error.
  virtual std::string GetHardwareClass() const = 0;

  // Returns the firmware version or an empty string if the system is
  // not running chrome os firmware.
  virtual std::string GetFirmwareVersion() const = 0;

  // Returns the ec version or an empty string if the system is not
  // running a custom chrome os ec.
  virtual std::string GetECVersion() const = 0;

  // Returns the powerwash_count from the stateful. If the file is not found
  // or is invalid, returns -1. Brand new machines out of the factory or after
  // recovery don't have this value set.
  virtual int GetPowerwashCount() const = 0;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_HARDWARE_INTERFACE_H_
