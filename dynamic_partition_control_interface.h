//
// Copyright (C) 2018 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_INTERFACE_H_
#define UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_INTERFACE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include <base/files/file_util.h>
#include <libdm/dm.h>
#include <liblp/builder.h>

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

struct FeatureFlag {
  enum class Value { NONE = 0, RETROFIT, LAUNCH };
  constexpr explicit FeatureFlag(Value value) : value_(value) {}
  constexpr bool IsEnabled() const { return value_ != Value::NONE; }
  constexpr bool IsRetrofit() const { return value_ == Value::RETROFIT; }

 private:
  Value value_;
};

class DynamicPartitionControlInterface {
 public:
  virtual ~DynamicPartitionControlInterface() = default;

  // Return the feature flags of dynamic partitions on this device.
  // Return RETROFIT iff dynamic partitions is retrofitted on this device,
  //        LAUNCH iff this device is launched with dynamic partitions,
  //        NONE iff dynamic partitions is disabled on this device.
  virtual FeatureFlag GetDynamicPartitionsFeatureFlag() = 0;

  // Return the feature flags of Virtual A/B on this device.
  virtual FeatureFlag GetVirtualAbFeatureFlag() = 0;

  // Map logical partition on device-mapper.
  // |super_device| is the device path of the physical partition ("super").
  // |target_partition_name| is the identifier used in metadata; for example,
  // "vendor_a"
  // |slot| is the selected slot to mount; for example, 0 for "_a".
  // Returns true if mapped successfully; if so, |path| is set to the device
  // path of the mapped logical partition.
  virtual bool MapPartitionOnDeviceMapper(
      const std::string& super_device,
      const std::string& target_partition_name,
      uint32_t slot,
      bool force_writable,
      std::string* path) = 0;

  // Do necessary cleanups before destroying the object.
  virtual void Cleanup() = 0;

  // Return true if a static partition exists at device path |path|.
  virtual bool DeviceExists(const std::string& path) = 0;

  // Returns the current state of the underlying device mapper device
  // with given name.
  // One of INVALID, SUSPENDED or ACTIVE.
  virtual android::dm::DmDeviceState GetState(const std::string& name) = 0;

  // Returns the path to the device mapper device node in '/dev' corresponding
  // to 'name'. If the device does not exist, false is returned, and the path
  // parameter is not set.
  virtual bool GetDmDevicePathByName(const std::string& name,
                                     std::string* path) = 0;

  // Retrieve metadata from |super_device| at slot |source_slot|.
  virtual std::unique_ptr<android::fs_mgr::MetadataBuilder> LoadMetadataBuilder(
      const std::string& super_device, uint32_t source_slot) = 0;

  // Prepare all partitions for an update specified in |manifest|.
  // This is needed before calling MapPartitionOnDeviceMapper(), otherwise the
  // device would be mapped in an inconsistent way.
  // If |update| is set, create snapshots and writes super partition metadata.
  virtual bool PreparePartitionsForUpdate(uint32_t source_slot,
                                          uint32_t target_slot,
                                          const DeltaArchiveManifest& manifest,
                                          bool update) = 0;

  // Return a possible location for devices listed by name.
  virtual bool GetDeviceDir(std::string* path) = 0;

  // Return the name of the super partition (which stores super partition
  // metadata) for a given slot.
  virtual std::string GetSuperPartitionName(uint32_t slot) = 0;

  virtual bool FinishUpdate() = 0;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_INTERFACE_H_
