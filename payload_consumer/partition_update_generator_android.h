//
// Copyright (C) 2020 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_PAYLOAD_CONSUMER_PARTITION_UPDATE_GENERATOR_ANDROID_H_
#define UPDATE_ENGINE_PAYLOAD_CONSUMER_PARTITION_UPDATE_GENERATOR_ANDROID_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include <brillo/secure_blob.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/payload_consumer/partition_update_generator_interface.h"

namespace chromeos_update_engine {
class PartitionUpdateGeneratorAndroid
    : public PartitionUpdateGeneratorInterface {
 public:
  PartitionUpdateGeneratorAndroid(BootControlInterface* boot_control,
                                  std::string device_dir,
                                  size_t block_size);

  bool GenerateOperationsForPartitionsNotInPayload(
      BootControlInterface::Slot source_slot,
      BootControlInterface::Slot target_slot,
      const std::set<std::string>& partitions_in_payload,
      std::vector<PartitionUpdate>* update_list) override;

 private:
  friend class PartitionUpdateGeneratorAndroidTest;
  FRIEND_TEST(PartitionUpdateGeneratorAndroidTest, GetStaticPartitions);
  FRIEND_TEST(PartitionUpdateGeneratorAndroidTest, CreatePartitionUpdate);

  // Gets the name of the static a/b partitions on the device.
  std::optional<std::set<std::string>> GetStaticAbPartitionsOnDevice();

  // Creates a PartitionUpdate object for a given partition to update from
  // source to target. Returns std::nullopt on failure.
  std::optional<PartitionUpdate> CreatePartitionUpdate(
      const std::string& partition_name,
      const std::string& source_device,
      const std::string& target_device,
      int64_t partition_size,
      bool is_dynamic);

  std::optional<PartitionUpdate> CreatePartitionUpdate(
      const std::string& partition_name,
      BootControlInterface::Slot source_slot,
      BootControlInterface::Slot target_slot);

  std::optional<brillo::Blob> CalculateHashForPartition(
      const std::string& block_device, int64_t partition_size);

  BootControlInterface* boot_control_;
  // Path to look for a/b partitions
  std::string block_device_dir_;
  size_t block_size_;
};

}  // namespace chromeos_update_engine

#endif
