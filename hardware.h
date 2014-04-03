// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_HARDWARE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_HARDWARE_H_

#include "update_engine/hardware_interface.h"

#include "base/basictypes.h"

namespace chromeos_update_engine {

// Implements the real interface with the hardware.
class Hardware : public HardwareInterface {
 public:
  Hardware();
  virtual ~Hardware() override;

  // HardwareInterface methods.
  virtual std::string BootKernelDevice() const override;
  virtual std::string BootDevice() const override;
  virtual std::vector<std::string> GetKernelDevices() const override;
  virtual bool IsKernelBootable(const std::string& kernel_device,
                                bool* bootable) const override;
  virtual bool MarkKernelUnbootable(const std::string& kernel_device) override;
  virtual bool IsOfficialBuild() const override;
  virtual bool IsNormalBootMode() const override;
  virtual bool IsOOBEComplete(base::Time* out_time_of_oobe) const override;
  virtual std::string GetHardwareClass() const override;
  virtual std::string GetFirmwareVersion() const override;
  virtual std::string GetECVersion() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Hardware);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_HARDWARE_H_
