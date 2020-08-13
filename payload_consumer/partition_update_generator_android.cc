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

#include "update_engine/payload_consumer/partition_update_generator_android.h"

#include <filesystem>
#include <memory>
#include <utility>

#include <android-base/properties.h>
#include <android-base/strings.h>
#include <base/logging.h>
#include <base/strings/string_split.h>

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/utils.h"

namespace chromeos_update_engine {

PartitionUpdateGeneratorAndroid::PartitionUpdateGeneratorAndroid(
    BootControlInterface* boot_control,
    size_t block_size)
    : boot_control_(boot_control),
      block_size_(block_size) {}

bool PartitionUpdateGeneratorAndroid::
    GenerateOperationsForPartitionsNotInPayload(
        BootControlInterface::Slot source_slot,
        BootControlInterface::Slot target_slot,
        const std::set<std::string>& partitions_in_payload,
        std::vector<PartitionUpdate>* update_list) {
  auto ab_partitions = GetAbPartitionsOnDevice();
  if (ab_partitions.empty()) {
    LOG(ERROR) << "Failed to load static a/b partitions";
    return false;
  }

  std::vector<PartitionUpdate> partition_updates;
  for (const auto& partition_name : ab_partitions) {
    if (partitions_in_payload.find(partition_name) !=
        partitions_in_payload.end()) {
      LOG(INFO) << partition_name << " has included in payload";
      continue;
    }
    bool is_source_dynamic = false;
    std::string source_device;

    TEST_AND_RETURN_FALSE(
        boot_control_->GetPartitionDevice(partition_name,
                                          source_slot,
                                          true, /* not_in_payload */
                                          &source_device,
                                          &is_source_dynamic));
    bool is_target_dynamic = false;
    std::string target_device;
    TEST_AND_RETURN_FALSE(boot_control_->GetPartitionDevice(
        partition_name, target_slot, true, &target_device, &is_target_dynamic));

    if (is_source_dynamic || is_target_dynamic) {
      if (is_source_dynamic != is_target_dynamic) {
        LOG(ERROR) << "Partition " << partition_name << " is expected to be a"
                   << " static partition. source slot is "
                   << (is_source_dynamic ? "" : "not")
                   << " dynamic, and target slot " << target_slot << " is "
                   << (is_target_dynamic ? "" : "not") << " dynamic.";
        return false;
      } else {
        continue;
      }
    }

    auto source_size = utils::FileSize(source_device);
    auto target_size = utils::FileSize(target_device);
    if (source_size == -1 || target_size == -1 || source_size != target_size ||
        source_size % block_size_ != 0) {
      LOG(ERROR) << "Invalid partition size. source size " << source_size
                 << ", target size " << target_size;
      return false;
    }

    auto partition_update = CreatePartitionUpdate(
        partition_name, source_device, target_device, source_size);
    if (!partition_update.has_value()) {
      LOG(ERROR) << "Failed to create partition update for " << partition_name;
      return false;
    }
    partition_updates.push_back(partition_update.value());
  }
  *update_list = std::move(partition_updates);
  return true;
}

std::vector<std::string>
PartitionUpdateGeneratorAndroid::GetAbPartitionsOnDevice() const {
  auto partition_list_str =
      android::base::GetProperty("ro.product.ab_ota_partitions", "");
  return base::SplitString(partition_list_str,
                           ",",
                           base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::optional<PartitionUpdate>
PartitionUpdateGeneratorAndroid::CreatePartitionUpdate(
    const std::string& partition_name,
    const std::string& source_device,
    const std::string& target_device,
    int64_t partition_size) {
  PartitionUpdate partition_update;
  partition_update.set_partition_name(partition_name);
  auto old_partition_info = partition_update.mutable_old_partition_info();
  old_partition_info->set_size(partition_size);

  auto raw_hash = CalculateHashForPartition(source_device, partition_size);
  if (!raw_hash.has_value()) {
    LOG(ERROR) << "Failed to calculate hash for partition " << source_device
               << " size: " << partition_size;
    return {};
  }
  old_partition_info->set_hash(raw_hash->data(), raw_hash->size());
  auto new_partition_info = partition_update.mutable_new_partition_info();
  new_partition_info->set_size(partition_size);
  new_partition_info->set_hash(raw_hash->data(), raw_hash->size());

  auto copy_operation = partition_update.add_operations();
  copy_operation->set_type(InstallOperation::SOURCE_COPY);
  Extent copy_extent;
  copy_extent.set_start_block(0);
  copy_extent.set_num_blocks(partition_size / block_size_);

  *copy_operation->add_src_extents() = copy_extent;
  *copy_operation->add_dst_extents() = copy_extent;

  return partition_update;
}

std::optional<brillo::Blob>
PartitionUpdateGeneratorAndroid::CalculateHashForPartition(
    const std::string& block_device, int64_t partition_size) {
  // TODO(xunchang) compute the hash with ecc partitions first, the hashing
  // behavior should match the one in SOURCE_COPY. Also, we don't have the
  // correct hash for source partition.
  // An alternative way is to verify the written bytes match the read bytes
  // during filesystem verification. This could probably save us a read of
  // partitions here.
  brillo::Blob raw_hash;
  if (HashCalculator::RawHashOfFile(block_device, partition_size, &raw_hash) !=
      partition_size) {
    LOG(ERROR) << "Failed to calculate hash for " << block_device;
    return std::nullopt;
  }

  return raw_hash;
}

namespace partition_update_generator {
std::unique_ptr<PartitionUpdateGeneratorInterface> Create(
    BootControlInterface* boot_control, size_t block_size) {
  CHECK(boot_control);

  return std::unique_ptr<PartitionUpdateGeneratorInterface>(
      new PartitionUpdateGeneratorAndroid(boot_control, block_size));
}
}  // namespace partition_update_generator

}  // namespace chromeos_update_engine
