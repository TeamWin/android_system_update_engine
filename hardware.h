// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_HARDWARE_H_
#define UPDATE_ENGINE_HARDWARE_H_

#include <string>
#include <vector>

#include <base/basictypes.h>

#include "update_engine/hardware_interface.h"

namespace chromeos_update_engine {

// Implements the real interface with the hardware.
class Hardware : public HardwareInterface {
 public:
  Hardware();
  ~Hardware() override;

  // HardwareInterface methods.
  std::string BootKernelDevice() const override;
  std::string BootDevice() const override;
  bool IsBootDeviceRemovable() const override;
  std::vector<std::string> GetKernelDevices() const override;
  bool IsKernelBootable(const std::string& kernel_device,
                        bool* bootable) const override;
  bool MarkKernelUnbootable(const std::string& kernel_device) override;
  bool IsOfficialBuild() const override;
  bool IsNormalBootMode() const override;
  bool IsOOBEComplete(base::Time* out_time_of_oobe) const override;
  std::string GetHardwareClass() const override;
  std::string GetFirmwareVersion() const override;
  std::string GetECVersion() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Hardware);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_HARDWARE_H_
