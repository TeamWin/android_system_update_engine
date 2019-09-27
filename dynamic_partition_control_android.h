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

#ifndef UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_ANDROID_H_
#define UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_ANDROID_H_

#include "update_engine/dynamic_partition_control_interface.h"

#include <memory>
#include <set>
#include <string>

#include <libsnapshot/snapshot.h>

namespace chromeos_update_engine {

class DynamicPartitionControlAndroid : public DynamicPartitionControlInterface {
 public:
  DynamicPartitionControlAndroid();
  ~DynamicPartitionControlAndroid();
  FeatureFlag GetDynamicPartitionsFeatureFlag() override;
  FeatureFlag GetVirtualAbFeatureFlag() override;
  bool MapPartitionOnDeviceMapper(const std::string& super_device,
                                  const std::string& target_partition_name,
                                  uint32_t slot,
                                  bool force_writable,
                                  std::string* path) override;
  void Cleanup() override;
  bool DeviceExists(const std::string& path) override;
  android::dm::DmDeviceState GetState(const std::string& name) override;
  bool GetDmDevicePathByName(const std::string& name,
                             std::string* path) override;
  std::unique_ptr<android::fs_mgr::MetadataBuilder> LoadMetadataBuilder(
      const std::string& super_device, uint32_t source_slot) override;

  bool PreparePartitionsForUpdate(uint32_t source_slot,
                                  uint32_t target_slot,
                                  const DeltaArchiveManifest& manifest,
                                  bool update) override;
  bool GetDeviceDir(std::string* path) override;
  std::string GetSuperPartitionName(uint32_t slot) override;
  bool FinishUpdate() override;

 protected:
  // These functions are exposed for testing.

  // Unmap logical partition on device mapper. This is the reverse operation
  // of MapPartitionOnDeviceMapper.
  // Returns true if unmapped successfully.
  virtual bool UnmapPartitionOnDeviceMapper(
      const std::string& target_partition_name);

  // Retrieve metadata from |super_device| at slot |source_slot|.
  //
  // If |target_slot| != kInvalidSlot, before returning the metadata, this
  // function modifies the metadata so that during updates, the metadata can be
  // written to |target_slot|. In particular, on retrofit devices, the returned
  // metadata automatically includes block devices at |target_slot|.
  //
  // If |target_slot| == kInvalidSlot, this function returns metadata at
  // |source_slot| without modifying it. This is the same as
  // LoadMetadataBuilder(const std::string&, uint32_t).
  virtual std::unique_ptr<android::fs_mgr::MetadataBuilder> LoadMetadataBuilder(
      const std::string& super_device,
      uint32_t source_slot,
      uint32_t target_slot);

  // Write metadata |builder| to |super_device| at slot |target_slot|.
  virtual bool StoreMetadata(const std::string& super_device,
                             android::fs_mgr::MetadataBuilder* builder,
                             uint32_t target_slot);

 private:
  friend class DynamicPartitionControlAndroidTest;

  void CleanupInternal(bool wait);
  bool MapPartitionInternal(const std::string& super_device,
                            const std::string& target_partition_name,
                            uint32_t slot,
                            bool force_writable,
                            std::string* path);

  // Update |builder| according to |partition_metadata|, assuming the device
  // does not have Virtual A/B.
  bool UpdatePartitionMetadata(android::fs_mgr::MetadataBuilder* builder,
                               uint32_t target_slot,
                               const DeltaArchiveManifest& manifest);

  // Helper for PreparePartitionsForUpdate. Used for dynamic partitions without
  // Virtual A/B update.
  bool PrepareDynamicPartitionsForUpdate(uint32_t source_slot,
                                         uint32_t target_slot,
                                         const DeltaArchiveManifest& manifest);

  // Helper for PreparePartitionsForUpdate. Used for snapshotted partitions for
  // Virtual A/B update.
  bool PrepareSnapshotPartitionsForUpdate(uint32_t source_slot,
                                          uint32_t target_slot,
                                          const DeltaArchiveManifest& manifest);

  std::set<std::string> mapped_devices_;
  std::unique_ptr<android::snapshot::SnapshotManager> snapshot_;
  bool target_supports_snapshot_ = false;

  DISALLOW_COPY_AND_ASSIGN(DynamicPartitionControlAndroid);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_ANDROID_H_
