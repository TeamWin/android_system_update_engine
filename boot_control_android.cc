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
#include <bootloader_message/bootloader_message.h>
#include <brillo/message_loops/message_loop.h>
#include <fs_mgr_overlayfs.h>
#include <libdm/dm.h>

#include "update_engine/common/utils.h"
#include "update_engine/dynamic_partition_control_android.h"

using std::string;

using android::dm::DmDeviceState;
using android::hardware::hidl_string;
using android::hardware::Return;
using android::hardware::boot::V1_0::BoolResult;
using android::hardware::boot::V1_0::CommandResult;
using android::hardware::boot::V1_0::IBootControl;
using Slot = chromeos_update_engine::BootControlInterface::Slot;

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
      device_dir.Append(dynamic_control_->GetSuperPartitionName(slot)).value();
  auto source_metadata =
      dynamic_control_->LoadMetadataBuilder(source_device, slot);
  return source_metadata->HasBlockDevice(partition_name_suffix);
}

BootControlAndroid::DynamicPartitionDeviceStatus
BootControlAndroid::GetDynamicPartitionDevice(
    const base::FilePath& device_dir,
    const string& partition_name_suffix,
    Slot slot,
    string* device) const {
  string super_device =
      device_dir.Append(dynamic_control_->GetSuperPartitionName(slot)).value();

  auto builder = dynamic_control_->LoadMetadataBuilder(super_device, slot);

  if (builder == nullptr) {
    LOG(ERROR) << "No metadata in slot "
               << BootControlInterface::SlotName(slot);
    return DynamicPartitionDeviceStatus::ERROR;
  }

  Slot current_slot = GetCurrentSlot();
  if (builder->FindPartition(partition_name_suffix) == nullptr) {
    LOG(INFO) << partition_name_suffix
              << " is not in super partition metadata.";

    if (IsSuperBlockDevice(device_dir, current_slot, partition_name_suffix)) {
      LOG(ERROR) << "The static partition " << partition_name_suffix
                 << " is a block device for current metadata ("
                 << dynamic_control_->GetSuperPartitionName(current_slot)
                 << ", slot " << BootControlInterface::SlotName(current_slot)
                 << "). It cannot be used as a logical partition.";
      return DynamicPartitionDeviceStatus::ERROR;
    }

    return DynamicPartitionDeviceStatus::TRY_STATIC;
  }

  if (slot == current_slot) {
    if (dynamic_control_->GetState(partition_name_suffix) !=
        DmDeviceState::ACTIVE) {
      LOG(WARNING) << partition_name_suffix << " is at current slot but it is "
                   << "not mapped. Now try to map it.";
    } else {
      if (dynamic_control_->GetDmDevicePathByName(partition_name_suffix,
                                                  device)) {
        LOG(INFO) << partition_name_suffix
                  << " is mapped on device mapper: " << *device;
        return DynamicPartitionDeviceStatus::SUCCESS;
      }
      LOG(ERROR) << partition_name_suffix << "is mapped but path is unknown.";
      return DynamicPartitionDeviceStatus::ERROR;
    }
  }

  bool force_writable = slot != current_slot;
  if (dynamic_control_->MapPartitionOnDeviceMapper(
          super_device, partition_name_suffix, slot, force_writable, device)) {
    return DynamicPartitionDeviceStatus::SUCCESS;
  }
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

  // When looking up target partition devices, treat them as static if the
  // current payload doesn't encode them as dynamic partitions. This may happen
  // when applying a retrofit update on top of a dynamic-partitions-enabled
  // build.
  if (dynamic_control_->GetDynamicPartitionsFeatureFlag().IsEnabled() &&
      (slot == GetCurrentSlot() || is_target_dynamic_)) {
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
               << " is bootable: " << ret.description();
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
               << SlotName(slot) << ": " << ret.description();
    return false;
  }
  if (!result.success) {
    LOG(ERROR) << "Unable to mark slot " << SlotName(slot)
               << " as unbootable: " << result.errMsg.c_str();
  }
  return result.success;
}

bool BootControlAndroid::SetActiveBootSlot(Slot slot) {
  if (slot != GetCurrentSlot() && !dynamic_control_->FinishUpdate()) {
    return false;
  }

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
    LOG(ERROR) << "Unable to call MarkBootSuccessful: " << ret.description();
    return false;
  }
  if (!result.success) {
    LOG(ERROR) << "Unable to mark boot successful: " << result.errMsg.c_str();
  }
  return brillo::MessageLoop::current()->PostTask(
             FROM_HERE, base::Bind(callback, result.success)) !=
         brillo::MessageLoop::kTaskIdNull;
}

bool BootControlAndroid::PreparePartitionsForUpdate(
    Slot target_slot,
    const DeltaArchiveManifest& manifest,
    bool update_metadata) {
  if (fs_mgr_overlayfs_is_setup()) {
    // Non DAP devices can use overlayfs as well.
    LOG(WARNING)
        << "overlayfs overrides are active and can interfere with our "
           "resources.\n"
        << "run adb enable-verity to deactivate if required and try again.";
  }
  if (!dynamic_control_->GetDynamicPartitionsFeatureFlag().IsEnabled()) {
    return true;
  }

  auto source_slot = GetCurrentSlot();
  if (target_slot == source_slot) {
    LOG(ERROR) << "Cannot call PreparePartitionsForUpdate on current slot.";
    return false;
  }

  // Although the current build supports dynamic partitions, the given payload
  // doesn't use it for target partitions. This could happen when applying a
  // retrofit update. Skip updating the partition metadata for the target slot.
  is_target_dynamic_ = !manifest.dynamic_partition_metadata().groups().empty();
  if (!is_target_dynamic_) {
    return true;
  }

  return dynamic_control_->PreparePartitionsForUpdate(
      source_slot, target_slot, manifest, update_metadata);
}

}  // namespace chromeos_update_engine
