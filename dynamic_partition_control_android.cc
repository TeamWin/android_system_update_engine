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

#include "update_engine/dynamic_partition_control_android.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <android-base/properties.h>
#include <android-base/strings.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <bootloader_message/bootloader_message.h>
#include <fs_mgr.h>
#include <fs_mgr_dm_linear.h>

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/utils.h"
#include "update_engine/dynamic_partition_utils.h"

using android::base::GetBoolProperty;
using android::base::Join;
using android::dm::DeviceMapper;
using android::dm::DmDeviceState;
using android::fs_mgr::CreateLogicalPartition;
using android::fs_mgr::CreateLogicalPartitionParams;
using android::fs_mgr::DestroyLogicalPartition;
using android::fs_mgr::MetadataBuilder;
using android::fs_mgr::Partition;
using android::fs_mgr::PartitionOpener;
using android::fs_mgr::SlotSuffixForSlotNumber;

namespace chromeos_update_engine {

using PartitionMetadata = BootControlInterface::PartitionMetadata;

constexpr char kUseDynamicPartitions[] = "ro.boot.dynamic_partitions";
constexpr char kRetrfoitDynamicPartitions[] =
    "ro.boot.dynamic_partitions_retrofit";
constexpr uint64_t kMapTimeoutMillis = 1000;

DynamicPartitionControlAndroid::~DynamicPartitionControlAndroid() {
  CleanupInternal(false /* wait */);
}

static FeatureFlag GetFeatureFlag(const char* enable_prop,
                                  const char* retrofit_prop) {
  bool retrofit = GetBoolProperty(retrofit_prop, false);
  bool enabled = GetBoolProperty(enable_prop, false);
  if (retrofit && !enabled) {
    LOG(ERROR) << retrofit_prop << " is true but " << enable_prop
               << " is not. These sysprops are inconsistent. Assume that "
               << enable_prop << " is true from now on.";
  }
  if (retrofit) {
    return FeatureFlag(FeatureFlag::Value::RETROFIT);
  }
  if (enabled) {
    return FeatureFlag(FeatureFlag::Value::LAUNCH);
  }
  return FeatureFlag(FeatureFlag::Value::NONE);
}

FeatureFlag DynamicPartitionControlAndroid::GetDynamicPartitionsFeatureFlag() {
  return GetFeatureFlag(kUseDynamicPartitions, kRetrfoitDynamicPartitions);
}

bool DynamicPartitionControlAndroid::MapPartitionInternal(
    const std::string& super_device,
    const std::string& target_partition_name,
    uint32_t slot,
    bool force_writable,
    std::string* path) {
  CreateLogicalPartitionParams params = {
      .block_device = super_device,
      .metadata_slot = slot,
      .partition_name = target_partition_name,
      .force_writable = force_writable,
      .timeout_ms = std::chrono::milliseconds(kMapTimeoutMillis),
  };

  if (!CreateLogicalPartition(params, path)) {
    LOG(ERROR) << "Cannot map " << target_partition_name << " in "
               << super_device << " on device mapper.";
    return false;
  }
  LOG(INFO) << "Succesfully mapped " << target_partition_name
            << " to device mapper (force_writable = " << force_writable
            << "); device path at " << *path;
  mapped_devices_.insert(target_partition_name);
  return true;
}

bool DynamicPartitionControlAndroid::MapPartitionOnDeviceMapper(
    const std::string& super_device,
    const std::string& target_partition_name,
    uint32_t slot,
    bool force_writable,
    std::string* path) {
  DmDeviceState state = GetState(target_partition_name);
  if (state == DmDeviceState::ACTIVE) {
    if (mapped_devices_.find(target_partition_name) != mapped_devices_.end()) {
      if (GetDmDevicePathByName(target_partition_name, path)) {
        LOG(INFO) << target_partition_name
                  << " is mapped on device mapper: " << *path;
        return true;
      }
      LOG(ERROR) << target_partition_name << " is mapped but path is unknown.";
      return false;
    }
    // If target_partition_name is not in mapped_devices_ but state is ACTIVE,
    // the device might be mapped incorrectly before. Attempt to unmap it.
    // Note that for source partitions, if GetState() == ACTIVE, callers (e.g.
    // BootControlAndroid) should not call MapPartitionOnDeviceMapper, but
    // should directly call GetDmDevicePathByName.
    if (!UnmapPartitionOnDeviceMapper(target_partition_name)) {
      LOG(ERROR) << target_partition_name
                 << " is mapped before the update, and it cannot be unmapped.";
      return false;
    }
    state = GetState(target_partition_name);
    if (state != DmDeviceState::INVALID) {
      LOG(ERROR) << target_partition_name << " is unmapped but state is "
                 << static_cast<std::underlying_type_t<DmDeviceState>>(state);
      return false;
    }
  }
  if (state == DmDeviceState::INVALID) {
    return MapPartitionInternal(
        super_device, target_partition_name, slot, force_writable, path);
  }

  LOG(ERROR) << target_partition_name
             << " is mapped on device mapper but state is unknown: "
             << static_cast<std::underlying_type_t<DmDeviceState>>(state);
  return false;
}

bool DynamicPartitionControlAndroid::UnmapPartitionOnDeviceMapper(
    const std::string& target_partition_name) {
  if (DeviceMapper::Instance().GetState(target_partition_name) !=
      DmDeviceState::INVALID) {
    if (!DestroyLogicalPartition(target_partition_name)) {
      LOG(ERROR) << "Cannot unmap " << target_partition_name
                 << " from device mapper.";
      return false;
    }
    LOG(INFO) << "Successfully unmapped " << target_partition_name
              << " from device mapper.";
  }
  mapped_devices_.erase(target_partition_name);
  return true;
}

void DynamicPartitionControlAndroid::CleanupInternal(bool wait) {
  if (mapped_devices_.empty()) {
    return;
  }
  // UnmapPartitionOnDeviceMapper removes objects from mapped_devices_, hence
  // a copy is needed for the loop.
  std::set<std::string> mapped = mapped_devices_;
  LOG(INFO) << "Destroying [" << Join(mapped, ", ") << "] from device mapper";
  for (const auto& partition_name : mapped) {
    ignore_result(UnmapPartitionOnDeviceMapper(partition_name));
  }
}

void DynamicPartitionControlAndroid::Cleanup() {
  CleanupInternal(true /* wait */);
}

bool DynamicPartitionControlAndroid::DeviceExists(const std::string& path) {
  return base::PathExists(base::FilePath(path));
}

android::dm::DmDeviceState DynamicPartitionControlAndroid::GetState(
    const std::string& name) {
  return DeviceMapper::Instance().GetState(name);
}

bool DynamicPartitionControlAndroid::GetDmDevicePathByName(
    const std::string& name, std::string* path) {
  return DeviceMapper::Instance().GetDmDevicePathByName(name, path);
}

std::unique_ptr<MetadataBuilder>
DynamicPartitionControlAndroid::LoadMetadataBuilder(
    const std::string& super_device, uint32_t source_slot) {
  return LoadMetadataBuilder(
      super_device, source_slot, BootControlInterface::kInvalidSlot);
}

std::unique_ptr<MetadataBuilder>
DynamicPartitionControlAndroid::LoadMetadataBuilder(
    const std::string& super_device,
    uint32_t source_slot,
    uint32_t target_slot) {
  std::unique_ptr<MetadataBuilder> builder;
  if (target_slot == BootControlInterface::kInvalidSlot) {
    builder =
        MetadataBuilder::New(PartitionOpener(), super_device, source_slot);
  } else {
    builder = MetadataBuilder::NewForUpdate(
        PartitionOpener(), super_device, source_slot, target_slot);
  }

  if (builder == nullptr) {
    LOG(WARNING) << "No metadata slot "
                 << BootControlInterface::SlotName(source_slot) << " in "
                 << super_device;
    return nullptr;
  }
  LOG(INFO) << "Loaded metadata from slot "
            << BootControlInterface::SlotName(source_slot) << " in "
            << super_device;
  return builder;
}

bool DynamicPartitionControlAndroid::StoreMetadata(
    const std::string& super_device,
    MetadataBuilder* builder,
    uint32_t target_slot) {
  auto metadata = builder->Export();
  if (metadata == nullptr) {
    LOG(ERROR) << "Cannot export metadata to slot "
               << BootControlInterface::SlotName(target_slot) << " in "
               << super_device;
    return false;
  }

  if (GetDynamicPartitionsFeatureFlag().IsRetrofit()) {
    if (!FlashPartitionTable(super_device, *metadata)) {
      LOG(ERROR) << "Cannot write metadata to " << super_device;
      return false;
    }
    LOG(INFO) << "Written metadata to " << super_device;
  } else {
    if (!UpdatePartitionTable(super_device, *metadata, target_slot)) {
      LOG(ERROR) << "Cannot write metadata to slot "
                 << BootControlInterface::SlotName(target_slot) << " in "
                 << super_device;
      return false;
    }
    LOG(INFO) << "Copied metadata to slot "
              << BootControlInterface::SlotName(target_slot) << " in "
              << super_device;
  }

  return true;
}

bool DynamicPartitionControlAndroid::GetDeviceDir(std::string* out) {
  // We can't use fs_mgr to look up |partition_name| because fstab
  // doesn't list every slot partition (it uses the slotselect option
  // to mask the suffix).
  //
  // We can however assume that there's an entry for the /misc mount
  // point and use that to get the device file for the misc
  // partition. This helps us locate the disk that |partition_name|
  // resides on. From there we'll assume that a by-name scheme is used
  // so we can just replace the trailing "misc" by the given
  // |partition_name| and suffix corresponding to |slot|, e.g.
  //
  //   /dev/block/platform/soc.0/7824900.sdhci/by-name/misc ->
  //   /dev/block/platform/soc.0/7824900.sdhci/by-name/boot_a
  //
  // If needed, it's possible to relax the by-name assumption in the
  // future by trawling /sys/block looking for the appropriate sibling
  // of misc and then finding an entry in /dev matching the sysfs
  // entry.

  std::string err, misc_device = get_bootloader_message_blk_device(&err);
  if (misc_device.empty()) {
    LOG(ERROR) << "Unable to get misc block device: " << err;
    return false;
  }

  if (!utils::IsSymlink(misc_device.c_str())) {
    LOG(ERROR) << "Device file " << misc_device << " for /misc "
               << "is not a symlink.";
    return false;
  }
  *out = base::FilePath(misc_device).DirName().value();
  return true;
}

bool DynamicPartitionControlAndroid::PreparePartitionsForUpdate(
    uint32_t source_slot,
    uint32_t target_slot,
    const PartitionMetadata& partition_metadata) {
  const std::string target_suffix = SlotSuffixForSlotNumber(target_slot);

  // Unmap all the target dynamic partitions because they would become
  // inconsistent with the new metadata.
  for (const auto& group : partition_metadata.groups) {
    for (const auto& partition : group.partitions) {
      if (!UnmapPartitionOnDeviceMapper(partition.name + target_suffix)) {
        return false;
      }
    }
  }

  std::string device_dir_str;
  if (!GetDeviceDir(&device_dir_str)) {
    return false;
  }
  base::FilePath device_dir(device_dir_str);
  auto source_device =
      device_dir.Append(GetSuperPartitionName(source_slot)).value();

  auto builder = LoadMetadataBuilder(source_device, source_slot, target_slot);
  if (builder == nullptr) {
    LOG(ERROR) << "No metadata at "
               << BootControlInterface::SlotName(source_slot);
    return false;
  }

  if (!UpdatePartitionMetadata(
          builder.get(), target_slot, partition_metadata)) {
    return false;
  }

  auto target_device =
      device_dir.Append(GetSuperPartitionName(target_slot)).value();
  return StoreMetadata(target_device, builder.get(), target_slot);
}

std::string DynamicPartitionControlAndroid::GetSuperPartitionName(
    uint32_t slot) {
  return fs_mgr_get_super_partition_name(slot);
}

bool DynamicPartitionControlAndroid::UpdatePartitionMetadata(
    MetadataBuilder* builder,
    uint32_t target_slot,
    const PartitionMetadata& partition_metadata) {
  const std::string target_suffix = SlotSuffixForSlotNumber(target_slot);
  DeleteGroupsWithSuffix(builder, target_suffix);

  uint64_t total_size = 0;
  for (const auto& group : partition_metadata.groups) {
    total_size += group.size;
  }

  std::string expr;
  uint64_t allocatable_space = builder->AllocatableSpace();
  if (!GetDynamicPartitionsFeatureFlag().IsRetrofit()) {
    allocatable_space /= 2;
    expr = "half of ";
  }
  if (total_size > allocatable_space) {
    LOG(ERROR) << "The maximum size of all groups with suffix " << target_suffix
               << " (" << total_size << ") has exceeded " << expr
               << "allocatable space for dynamic partitions "
               << allocatable_space << ".";
    return false;
  }

  for (const auto& group : partition_metadata.groups) {
    auto group_name_suffix = group.name + target_suffix;
    if (!builder->AddGroup(group_name_suffix, group.size)) {
      LOG(ERROR) << "Cannot add group " << group_name_suffix << " with size "
                 << group.size;
      return false;
    }
    LOG(INFO) << "Added group " << group_name_suffix << " with size "
              << group.size;

    for (const auto& partition : group.partitions) {
      auto partition_name_suffix = partition.name + target_suffix;
      Partition* p = builder->AddPartition(
          partition_name_suffix, group_name_suffix, LP_PARTITION_ATTR_READONLY);
      if (!p) {
        LOG(ERROR) << "Cannot add partition " << partition_name_suffix
                   << " to group " << group_name_suffix;
        return false;
      }
      if (!builder->ResizePartition(p, partition.size)) {
        LOG(ERROR) << "Cannot resize partition " << partition_name_suffix
                   << " to size " << partition.size << ". Not enough space?";
        return false;
      }
      LOG(INFO) << "Added partition " << partition_name_suffix << " to group "
                << group_name_suffix << " with size " << partition.size;
    }
  }

  return true;
}

}  // namespace chromeos_update_engine
