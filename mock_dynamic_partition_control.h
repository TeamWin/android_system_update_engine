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

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include <gmock/gmock.h>

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/dynamic_partition_control_interface.h"
#include "update_engine/dynamic_partition_control_android.h"

namespace chromeos_update_engine {

class MockDynamicPartitionControlAndroid
    : public DynamicPartitionControlAndroid {
 public:
  MOCK_METHOD(
      bool,
      MapPartitionOnDeviceMapper,
      (const std::string&, const std::string&, uint32_t, bool, std::string*),
      (override));
  MOCK_METHOD(bool,
              UnmapPartitionOnDeviceMapper,
              (const std::string&),
              (override));
  MOCK_METHOD(void, Cleanup, (), (override));
  MOCK_METHOD(bool, DeviceExists, (const std::string&), (override));
  MOCK_METHOD(::android::dm::DmDeviceState,
              GetState,
              (const std::string&),
              (override));
  MOCK_METHOD(bool,
              GetDmDevicePathByName,
              (const std::string&, std::string*),
              (override));
  MOCK_METHOD(std::unique_ptr<::android::fs_mgr::MetadataBuilder>,
              LoadMetadataBuilder,
              (const std::string&, uint32_t),
              (override));
  MOCK_METHOD(std::unique_ptr<::android::fs_mgr::MetadataBuilder>,
              LoadMetadataBuilder,
              (const std::string&, uint32_t, uint32_t),
              (override));
  MOCK_METHOD(bool,
              StoreMetadata,
              (const std::string&, android::fs_mgr::MetadataBuilder*, uint32_t),
              (override));
  MOCK_METHOD(bool, GetDeviceDir, (std::string*), (override));
  MOCK_METHOD(FeatureFlag, GetDynamicPartitionsFeatureFlag, (), (override));
  MOCK_METHOD(std::string, GetSuperPartitionName, (uint32_t), (override));
  MOCK_METHOD(FeatureFlag, GetVirtualAbFeatureFlag, (), (override));
  MOCK_METHOD(bool, FinishUpdate, (bool), (override));
  MOCK_METHOD(bool,
              GetSystemOtherPath,
              (uint32_t, uint32_t, const std::string&, std::string*, bool*),
              (override));
  MOCK_METHOD(bool,
              EraseSystemOtherAvbFooter,
              (uint32_t, uint32_t),
              (override));
  MOCK_METHOD(std::optional<bool>, IsAvbEnabledOnSystemOther, (), (override));
  MOCK_METHOD(bool, IsRecovery, (), (override));
  MOCK_METHOD(bool,
              PrepareDynamicPartitionsForUpdate,
              (uint32_t, uint32_t, const DeltaArchiveManifest&, bool),
              (override));

  void set_fake_mapped_devices(const std::set<std::string>& fake) override {
    DynamicPartitionControlAndroid::set_fake_mapped_devices(fake);
  }

  bool RealGetSystemOtherPath(uint32_t source_slot,
                              uint32_t target_slot,
                              const std::string& partition_name_suffix,
                              std::string* path,
                              bool* should_unmap) {
    return DynamicPartitionControlAndroid::GetSystemOtherPath(
        source_slot, target_slot, partition_name_suffix, path, should_unmap);
  }

  bool RealEraseSystemOtherAvbFooter(uint32_t source_slot,
                                     uint32_t target_slot) {
    return DynamicPartitionControlAndroid::EraseSystemOtherAvbFooter(
        source_slot, target_slot);
  }

  std::optional<bool> RealIsAvbEnabledInFstab(const std::string& path) {
    return DynamicPartitionControlAndroid::IsAvbEnabledInFstab(path);
  }

  bool RealPrepareDynamicPartitionsForUpdate(
      uint32_t source_slot,
      uint32_t target_slot,
      const DeltaArchiveManifest& manifest,
      bool delete_source) {
    return DynamicPartitionControlAndroid::PrepareDynamicPartitionsForUpdate(
        source_slot, target_slot, manifest, delete_source);
  }
};

}  // namespace chromeos_update_engine
