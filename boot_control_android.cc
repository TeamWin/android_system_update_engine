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

#include <base/bind.h>
#include <base/logging.h>
#include <bootloader_message/bootloader_message.h>
#include <brillo/message_loops/message_loop.h>
#include <fs_mgr.h>

#include "update_engine/common/utils.h"
#include "update_engine/dynamic_partition_control_android.h"

using std::string;

using android::dm::DmDeviceState;
using android::fs_mgr::MetadataBuilder;
using android::fs_mgr::Partition;
using android::fs_mgr::UpdatePartitionTable;
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

bool BootControlAndroid::GetPartitionDevice(const string& partition_name,
                                            Slot slot,
                                            string* device) const {
  string suffix;
  if (!GetSuffix(slot, &suffix)) {
    return false;
  }

  const string target_partition_name = partition_name + suffix;

  // DeltaPerformer calls InitPartitionMetadata before calling
  // InstallPlan::LoadPartitionsFromSlots. After InitPartitionMetadata,
  // the partition must be re-mapped with force_writable == true. Hence,
  // we only need to check device mapper.
  if (dynamic_control_->IsDynamicPartitionsEnabled()) {
    switch (dynamic_control_->GetState(target_partition_name)) {
      case DmDeviceState::ACTIVE:
        if (dynamic_control_->GetDmDevicePathByName(target_partition_name,
                                                    device)) {
          LOG(INFO) << target_partition_name
                    << " is mapped on device mapper: " << *device;
          return true;
        }
        LOG(ERROR) << target_partition_name
                   << " is mapped but path is unknown.";
        return false;

      case DmDeviceState::INVALID:
        // Try static partitions.
        break;

      case DmDeviceState::SUSPENDED:  // fallthrough
      default:
        LOG(ERROR) << target_partition_name
                   << " is mapped on device mapper but state is unknown";
        return false;
    }
  }

  string device_dir_str;
  if (!dynamic_control_->GetDeviceDir(&device_dir_str)) {
    return false;
  }

  base::FilePath path =
      base::FilePath(device_dir_str).Append(target_partition_name);
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

// Resize |partition_name|_|slot| to the given |size|.
bool ResizePartition(MetadataBuilder* builder,
                     const string& target_partition_name,
                     uint64_t size) {
  Partition* partition = builder->FindPartition(target_partition_name);
  if (partition == nullptr) {
    LOG(ERROR) << "Cannot find " << target_partition_name << " in metadata.";
    return false;
  }

  uint64_t old_size = partition->size();
  const string action = "resize " + target_partition_name + " in super (" +
                        std::to_string(old_size) + " -> " +
                        std::to_string(size) + " bytes)";
  if (!builder->ResizePartition(partition, size)) {
    LOG(ERROR) << "Cannot " << action << "; see previous log messages.";
    return false;
  }

  if (partition->size() != size) {
    LOG(ERROR) << "Cannot " << action
               << "; value is misaligned and partition should have been "
               << partition->size();
    return false;
  }

  LOG(INFO) << "Successfully " << action;

  return true;
}

bool ResizePartitions(DynamicPartitionControlInterface* dynamic_control,
                      const string& super_device,
                      Slot target_slot,
                      const string& target_suffix,
                      const PartitionMetadata& logical_sizes,
                      MetadataBuilder* builder) {
  // Delete all extents to ensure that each partition has enough space to
  // grow.
  for (const auto& pair : logical_sizes) {
    const string target_partition_name = pair.first + target_suffix;
    if (builder->FindPartition(target_partition_name) == nullptr) {
      // Use constant GUID because it is unused.
      LOG(INFO) << "Adding partition " << target_partition_name << " to slot "
                << BootControlInterface::SlotName(target_slot) << " in "
                << super_device;
      if (builder->AddPartition(target_partition_name,
                                LP_PARTITION_ATTR_READONLY) == nullptr) {
        LOG(ERROR) << "Cannot add partition " << target_partition_name;
        return false;
      }
    }
    if (!ResizePartition(builder, pair.first + target_suffix, 0 /* size */)) {
      return false;
    }
  }

  for (const auto& pair : logical_sizes) {
    if (!ResizePartition(builder, pair.first + target_suffix, pair.second)) {
      LOG(ERROR) << "Not enough space?";
      return false;
    }
  }

  if (!dynamic_control->StoreMetadata(super_device, builder, target_slot)) {
    return false;
  }
  return true;
}

// Assume upgrading from slot A to B. A partition foo is considered dynamic
// iff one of the following:
// 1. foo_a exists as a dynamic partition (so it should continue to be a
//    dynamic partition)
// 2. foo_b does not exist as a static partition (in which case we may be
//    adding a new partition).
bool IsDynamicPartition(DynamicPartitionControlInterface* dynamic_control,
                        const base::FilePath& device_dir,
                        MetadataBuilder* source_metadata,
                        const string& partition_name,
                        const string& source_suffix,
                        const string& target_suffix) {
  bool dynamic_source_exist =
      source_metadata->FindPartition(partition_name + source_suffix) != nullptr;
  bool static_target_exist = dynamic_control->DeviceExists(
      device_dir.Append(partition_name + target_suffix).value());

  return dynamic_source_exist || !static_target_exist;
}

bool FilterPartitionSizes(DynamicPartitionControlInterface* dynamic_control,
                          const base::FilePath& device_dir,
                          const PartitionMetadata& partition_metadata,
                          MetadataBuilder* source_metadata,
                          const string& source_suffix,
                          const string& target_suffix,
                          PartitionMetadata* logical_sizes) {
  for (const auto& pair : partition_metadata) {
    if (!IsDynamicPartition(dynamic_control,
                            device_dir,
                            source_metadata,
                            pair.first,
                            source_suffix,
                            target_suffix)) {
      // In the future we can check static partition sizes, but skip for now.
      LOG(INFO) << pair.first << " is static; assume its size is "
                << pair.second << " bytes.";
      continue;
    }

    logical_sizes->insert(pair);
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
  string super_device =
      device_dir.Append(fs_mgr_get_super_partition_name()).value();

  Slot current_slot = GetCurrentSlot();
  if (target_slot == current_slot) {
    LOG(ERROR) << "Cannot call InitPartitionMetadata on current slot.";
    return false;
  }

  string current_suffix;
  if (!GetSuffix(current_slot, &current_suffix)) {
    return false;
  }

  string target_suffix;
  if (!GetSuffix(target_slot, &target_suffix)) {
    return false;
  }

  auto builder =
      dynamic_control_->LoadMetadataBuilder(super_device, current_slot);
  if (builder == nullptr) {
    return false;
  }

  // Read metadata from current slot to determine which partitions are logical
  // and may be resized. Do not read from target slot because metadata at
  // target slot may be corrupted.
  PartitionMetadata logical_sizes;
  if (!FilterPartitionSizes(dynamic_control_.get(),
                            device_dir,
                            partition_metadata,
                            builder.get() /* source metadata */,
                            current_suffix,
                            target_suffix,
                            &logical_sizes)) {
    return false;
  }

  if (!ResizePartitions(dynamic_control_.get(),
                        super_device,
                        target_slot,
                        target_suffix,
                        logical_sizes,
                        builder.get())) {
    return false;
  }

  // Unmap all partitions, and remap partitions if size is non-zero.
  for (const auto& pair : logical_sizes) {
    if (!dynamic_control_->UnmapPartitionOnDeviceMapper(
            pair.first + target_suffix, true /* wait */)) {
      return false;
    }
    if (pair.second == 0) {
      continue;
    }
    string map_path;
    if (!dynamic_control_->MapPartitionOnDeviceMapper(
            super_device, pair.first + target_suffix, target_slot, &map_path)) {
      return false;
    }
  }
  return true;
}

}  // namespace chromeos_update_engine
