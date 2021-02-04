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
#include <vector>

#include <gmock/gmock.h>

#include "update_engine/common/dynamic_partition_control_interface.h"
#include "update_engine/payload_consumer/file_descriptor.h"

namespace chromeos_update_engine {

class MockDynamicPartitionControl : public DynamicPartitionControlInterface {
 public:
  MOCK_METHOD(void, Cleanup, (), (override));
  MOCK_METHOD(bool, GetDeviceDir, (std::string*), (override));
  MOCK_METHOD(FeatureFlag, GetDynamicPartitionsFeatureFlag, (), (override));
  MOCK_METHOD(FeatureFlag, GetVirtualAbCompressionFeatureFlag, (), (override));
  MOCK_METHOD(FeatureFlag, GetVirtualAbFeatureFlag, (), (override));
  MOCK_METHOD(bool, FinishUpdate, (bool), (override));
  MOCK_METHOD(FileDescriptorPtr,
              OpenCowReader,
              (const std::string& unsuffixed_partition_name,
               const std::optional<std::string>& source_path,
               bool is_append),
              (override));
  MOCK_METHOD(bool, MapAllPartitions, (), (override));
  MOCK_METHOD(bool, UnmapAllPartitions, (), (override));

  MOCK_METHOD(bool,
              OptimizeOperation,
              (const std::string&, const InstallOperation&, InstallOperation*),
              (override));

  std::unique_ptr<android::snapshot::ISnapshotWriter> OpenCowWriter(
      const std::string& unsuffixed_partition_name,
      const std::optional<std::string>&,
      bool is_append = false) override {
    return nullptr;
  }

  MOCK_METHOD(
      bool,
      PreparePartitionsForUpdate,
      (uint32_t, uint32_t, const DeltaArchiveManifest&, bool, uint64_t*),
      (override));

  MOCK_METHOD(bool, ResetUpdate, (PrefsInterface*), (override));
  MOCK_METHOD(std::unique_ptr<AbstractAction>,
              GetCleanupPreviousUpdateAction,
              (BootControlInterface*,
               PrefsInterface*,
               CleanupPreviousUpdateActionDelegateInterface*),
              (override));
  MOCK_METHOD(bool,
              ListDynamicPartitionsForSlot,
              (uint32_t, std::vector<std::string>*),
              (override));
  MOCK_METHOD(bool,
              VerifyExtentsForUntouchedPartitions,
              (uint32_t, uint32_t, const std::vector<std::string>&),
              (override));
  MOCK_METHOD(bool, IsDynamicPartition, (const std::string&), (override));
  MOCK_METHOD(bool, UpdateUsesSnapshotCompression, (), (override));
};

}  // namespace chromeos_update_engine
