//
// Copyright (C) 2015 The Android Open Source Project
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

#include "update_engine/boot_control_android.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <bootloader_message/bootloader_message.h>
#include <brillo/message_loops/message_loop.h>
#include <fs_mgr.h>

#include "update_engine/common/utils.h"
#include "update_engine/dynamic_partition_control_android.h"

using std::string;

using android::dm::DmDeviceState;
using android::fs_mgr::Partition;
using android::hardware::hidl_string;
using android::hardware::Return;
using android::hardware::boot::V1_0::BoolResult;
using android::hardware::boot::V1_0::CommandResult;
using android::hardware::boot::V1_0::IBootControl;
using Slot = chromeos_update_engine::BootControlInterface::Slot;
using PartitionMetadata =
    chromeos_update_engine::BootControlInterface::PartitionMetadata;

namespace {

auto StoreResultCallback(CommandResult* dest) {
  return [dest](const CommandResult& result) { *dest = result; };
}
}  // namespace

namespace chromeos_update_engine {

namespace boot_control {

// Factory defined in boot_control.h.
std::unique_ptr<BootControlInterface> CreateBootControl() {
  auto boot_control = std::make_unique<BootControlAndroid>();
  if (!boot_control->Init()) {
    return nullptr;
  }
  return std::move(boot_control);
}

}  // namespace boot_control

bool BootControlAndroid::Init() {
  module_ = IBootControl::getService();
  if (module_ == nullptr) {
    LOG(ERROR) << "Error getting bootctrl HIDL module.";
    return false;
  }

  LOG(INFO) << "Loaded boot control hidl hal.";

  dynamic_control_ = std::make_unique<DynamicPartitionControlAndroid>();

  return true;
}

void BootControlAndroid::Cleanup() {
  dynamic_control_->Cleanup();
}

unsigned int BootControlAndroid::GetNumSlots() const {
  return module_->getNumberSlots();
}

BootControlInterface::Slot BootControlAndroid::GetCurrentSlot() const {
  return module_->getCurrentSlot();
}

bool BootControlAndroid::GetSuffix(Slot slot, string* suffix) const {
  auto store_suffix_cb = [&suffix](hidl_string cb_suffix) {
    *suffix = cb_suffix.c_str();
  };
  Return<void> ret = module_->getSuffix(slot, store_suffix_cb);

  if (!ret.isOk()) {
    LOG(ERROR) << "boot_control impl returned no suffix for slot "
               << SlotName(slot);
    return false;
  }
  return true;
}

bool BootControlAndroid::IsSuperBlockDevice(
    const base::FilePath& device_dir,
    Slot slot,
    const string& partition_name_suffix) const {
  string source_device =
      device_dir.Append(fs_mgr_get_super_partition_name(slot)).value();
  auto source_metadata = dynamic_control_->LoadMetadataBuilder(
      source_device, slot, BootControlInterface::kInvalidSlot);
  return source_metadata->HasBlockDevice(partition_name_suffix);
}

BootControlAndroid::DynamicPartitionDeviceStatus
BootControlAndroid::GetDynamicPartitionDevice(
    const base::FilePath& device_dir,
    const string& partition_name_suffix,
    Slot slot,
    string* device) const {
  if (!dynamic_control_->IsDynamicPartitionsEnabled()) {
    return DynamicPartitionDeviceStatus::TRY_STATIC;
  }

  string super_device =
      device_dir.Append(fs_mgr_get_super_partition_name(slot)).value();

  auto builder = dynamic_control_->LoadMetadataBuilder(
      super_device, slot, BootControlInterface::kInvalidSlot);

  if (builder == nullptr) {
    LOG(ERROR) << "No metadata in slot "
               << BootControlInterface::SlotName(slot);
    return DynamicPartitionDeviceStatus::ERROR;
  }

  if (builder->FindPartition(partition_name_suffix) == nullptr) {
    LOG(INFO) << partition_name_suffix
              << " is not in super partition metadata.";

    Slot current_slot = GetCurrentSlot();
    if (IsSuperBlockDevice(device_dir, current_slot, partition_name_suffix)) {
      LOG(ERROR) << "The static partition " << partition_name_suffix
                 << " is a block device for current metadata ("
                 << fs_mgr_get_super_partition_name(current_slot) << ", slot "
                 << BootControlInterface::SlotName(current_slot)
                 << "). It cannot be used as a logical partition.";
      return DynamicPartitionDeviceStatus::ERROR;
    }

    return DynamicPartitionDeviceStatus::TRY_STATIC;
  }

  DmDeviceState state = dynamic_control_->GetState(partition_name_suffix);

  if (state == DmDeviceState::ACTIVE) {
    if (dynamic_control_->GetDmDevicePathByName(partition_name_suffix,
                                                device)) {
      LOG(INFO) << partition_name_suffix
                << " is mapped on device mapper: " << *device;
      return DynamicPartitionDeviceStatus::SUCCESS;
    }
    LOG(ERROR) << partition_name_suffix << " is mapped but path is unknown.";
    return DynamicPartitionDeviceStatus::ERROR;
  }

  // DeltaPerformer calls InitPartitionMetadata before calling
  // InstallPlan::LoadPartitionsFromSlots. After InitPartitionMetadata,
  // the target partition must be re-mapped with force_writable == true.
  // Hence, if it is not mapped, we assume it is a source partition and
  // map it without force_writable.
  if (state == DmDeviceState::INVALID) {
    if (dynamic_control_->MapPartitionOnDeviceMapper(super_device,
                                                     partition_name_suffix,
                                                     slot,
                                                     false /* force_writable */,
                                                     device)) {
      return DynamicPartitionDeviceStatus::SUCCESS;
    }
    return DynamicPartitionDeviceStatus::ERROR;
  }

  LOG(ERROR) << partition_name_suffix
             << " is mapped on device mapper but state is unknown: "
             << static_cast<std::underlying_type_t<DmDeviceState>>(state);
  return DynamicPartitionDeviceStatus::ERROR;
}

bool BootControlAndroid::GetPartitionDevice(const string& partition_name,
                                            Slot slot,
                                            string* device) const {
  string suffix;
  if (!GetSuffix(slot, &suffix)) {
    return false;
  }
  const string partition_name_suffix = partition_name + suffix;

  string device_dir_str;
  if (!dynamic_control_->GetDeviceDir(&device_dir_str)) {
    return false;
  }
  base::FilePath device_dir(device_dir_str);

  switch (GetDynamicPartitionDevice(
      device_dir, partition_name_suffix, slot, device)) {
    case DynamicPartitionDeviceStatus::SUCCESS:
      return true;
    case DynamicPartitionDeviceStatus::TRY_STATIC:
      break;
    case DynamicPartitionDeviceStatus::ERROR:  // fallthrough
    default:
      return false;
  }

  base::FilePath path = device_dir.Append(partition_name_suffix);
  if (!dynamic_control_->DeviceExists(path.value())) {
    LOG(ERROR) << "Device file " << path.value() << " does not exist.";
    return false;
  }

  *device = path.value();
  return true;
}

bool BootControlAndroid::IsSlotBootable(Slot slot) const {
  Return<BoolResult> ret = module_->isSlotBootable(slot);
  if (!ret.isOk()) {
    LOG(ERROR) << "Unable to determine if slot " << SlotName(slot)
               << " is bootable: "
               << ret.description();
    return false;
  }
  if (ret == BoolResult::INVALID_SLOT) {
    LOG(ERROR) << "Invalid slot: " << SlotName(slot);
    return false;
  }
  return ret == BoolResult::TRUE;
}

bool BootControlAndroid::MarkSlotUnbootable(Slot slot) {
  CommandResult result;
  auto ret = module_->setSlotAsUnbootable(slot, StoreResultCallback(&result));
  if (!ret.isOk()) {
    LOG(ERROR) << "Unable to call MarkSlotUnbootable for slot "
               << SlotName(slot) << ": "
               << ret.description();
    return false;
  }
  if (!result.success) {
    LOG(ERROR) << "Unable to mark slot " << SlotName(slot)
               << " as unbootable: " << result.errMsg.c_str();
  }
  return result.success;
}

bool BootControlAndroid::SetActiveBootSlot(Slot slot) {
  CommandResult result;
  auto ret = module_->setActiveBootSlot(slot, StoreResultCallback(&result));
  if (!ret.isOk()) {
    LOG(ERROR) << "Unable to call SetActiveBootSlot for slot " << SlotName(slot)
               << ": " << ret.description();
    return false;
  }
  if (!result.success) {
    LOG(ERROR) << "Unable to set the active slot to slot " << SlotName(slot)
               << ": " << result.errMsg.c_str();
  }
  return result.success;
}

bool BootControlAndroid::MarkBootSuccessfulAsync(
    base::Callback<void(bool)> callback) {
  CommandResult result;
  auto ret = module_->markBootSuccessful(StoreResultCallback(&result));
  if (!ret.isOk()) {
    LOG(ERROR) << "Unable to call MarkBootSuccessful: "
               << ret.description();
    return false;
  }
  if (!result.success) {
    LOG(ERROR) << "Unable to mark boot successful: " << result.errMsg.c_str();
  }
  return brillo::MessageLoop::current()->PostTask(
             FROM_HERE, base::Bind(callback, result.success)) !=
         brillo::MessageLoop::kTaskIdNull;
}

namespace {

bool InitPartitionMetadataInternal(
    DynamicPartitionControlInterface* dynamic_control,
    const string& source_device,
    const string& target_device,
    Slot source_slot,
    Slot target_slot,
    const string& target_suffix,
    const PartitionMetadata& partition_metadata) {
  auto builder = dynamic_control->LoadMetadataBuilder(
      source_device, source_slot, target_slot);
  if (builder == nullptr) {
    // TODO(elsk): allow reconstructing metadata from partition_metadata
    // in recovery sideload.
    LOG(ERROR) << "No metadata at "
               << BootControlInterface::SlotName(source_slot);
    return false;
  }

  std::vector<string> groups = builder->ListGroups();
  for (const auto& group_name : groups) {
    if (base::EndsWith(
            group_name, target_suffix, base::CompareCase::SENSITIVE)) {
      LOG(INFO) << "Removing group " << group_name;
      builder->RemoveGroupAndPartitions(group_name);
    }
  }

  uint64_t total_size = 0;
  for (const auto& group : partition_metadata.groups) {
    total_size += group.size;
  }

  if (total_size > (builder->AllocatableSpace() / 2)) {
    LOG(ERROR)
        << "The maximum size of all groups with suffix " << target_suffix
        << " (" << total_size
        << ") has exceeded half of allocatable space for dynamic partitions "
        << (builder->AllocatableSpace() / 2) << ".";
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

  return dynamic_control->StoreMetadata(
      target_device, builder.get(), target_slot);
}

// Unmap all partitions, and remap partitions as writable.
bool Remap(DynamicPartitionControlInterface* dynamic_control,
           const string& target_device,
           Slot target_slot,
           const string& target_suffix,
           const PartitionMetadata& partition_metadata) {
  for (const auto& group : partition_metadata.groups) {
    for (const auto& partition : group.partitions) {
      if (!dynamic_control->UnmapPartitionOnDeviceMapper(
              partition.name + target_suffix, true /* wait */)) {
        return false;
      }
      if (partition.size == 0) {
        continue;
      }
      string map_path;
      if (!dynamic_control->MapPartitionOnDeviceMapper(
              target_device,
              partition.name + target_suffix,
              target_slot,
              true /* force writable */,
              &map_path)) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

bool BootControlAndroid::InitPartitionMetadata(
    Slot target_slot, const PartitionMetadata& partition_metadata) {
  if (!dynamic_control_->IsDynamicPartitionsEnabled()) {
    return true;
  }

  string device_dir_str;
  if (!dynamic_control_->GetDeviceDir(&device_dir_str)) {
    return false;
  }
  base::FilePath device_dir(device_dir_str);
  string target_device =
      device_dir.Append(fs_mgr_get_super_partition_name(target_slot)).value();

  Slot current_slot = GetCurrentSlot();
  if (target_slot == current_slot) {
    LOG(ERROR) << "Cannot call InitPartitionMetadata on current slot.";
    return false;
  }
  string source_device =
      device_dir.Append(fs_mgr_get_super_partition_name(current_slot)).value();

  string target_suffix;
  if (!GetSuffix(target_slot, &target_suffix)) {
    return false;
  }

  if (!InitPartitionMetadataInternal(dynamic_control_.get(),
                                     source_device,
                                     target_device,
                                     current_slot,
                                     target_slot,
                                     target_suffix,
                                     partition_metadata)) {
    return false;
  }

  if (!Remap(dynamic_control_.get(),
             target_device,
             target_slot,
             target_suffix,
             partition_metadata)) {
    return false;
  }

  return true;
}

}  // namespace chromeos_update_engine
