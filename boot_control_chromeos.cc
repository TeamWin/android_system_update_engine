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

#include "update_engine/boot_control_chromeos.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <chromeos/make_unique_ptr.h>
#include <rootdev/rootdev.h>

extern "C" {
#include <vboot/vboot_host.h>
}

#include "update_engine/boot_control.h"
#include "update_engine/utils.h"

using std::string;

namespace {

const char* kChromeOSPartitionNameKernel = "kernel";
const char* kChromeOSPartitionNameRoot = "root";
const char* kAndroidPartitionNameKernel = "boot";
const char* kAndroidPartitionNameRoot = "system";

// Returns the currently booted rootfs partition. "/dev/sda3", for example.
string GetBootDevice() {
  char boot_path[PATH_MAX];
  // Resolve the boot device path fully, including dereferencing through
  // dm-verity.
  int ret = rootdev(boot_path, sizeof(boot_path), true, false);
  if (ret < 0) {
    LOG(ERROR) << "rootdev failed to find the root device";
    return "";
  }
  LOG_IF(WARNING, ret > 0) << "rootdev found a device name with no device node";

  // This local variable is used to construct the return string and is not
  // passed around after use.
  return boot_path;
}

}  // namespace

namespace chromeos_update_engine {

namespace boot_control {

// Factory defined in boot_control.h.
std::unique_ptr<BootControlInterface> CreateBootControl() {
  std::unique_ptr<BootControlChromeOS> boot_control_chromeos(
      new BootControlChromeOS());
  if (!boot_control_chromeos->Init()) {
    LOG(ERROR) << "Ignoring BootControlChromeOS failure. We won't run updates.";
  }
  return chromeos::make_unique_ptr(boot_control_chromeos.release());
}

}  // namespace boot_control

bool BootControlChromeOS::Init() {
  string boot_device = GetBootDevice();
  if (boot_device.empty())
    return false;

  int partition_num;
  if (!utils::SplitPartitionName(boot_device, &boot_disk_name_, &partition_num))
    return false;

  // All installed Chrome OS devices have two slots. We don't update removable
  // devices, so we will pretend we have only one slot in that case.
  if (IsRemovableDevice(boot_disk_name_)) {
    LOG(INFO)
        << "Booted from a removable device, pretending we have only one slot.";
    num_slots_ = 1;
  } else {
    // TODO(deymo): Look at the actual number of slots reported in the GPT.
    num_slots_ = 2;
  }

  // Search through the slots to see which slot has the partition_num we booted
  // from. This should map to one of the existing slots, otherwise something is
  // very wrong.
  current_slot_ = 0;
  while (current_slot_ < num_slots_ &&
         partition_num !=
             GetPartitionNumber(kChromeOSPartitionNameRoot, current_slot_)) {
    current_slot_++;
  }
  if (current_slot_ >= num_slots_) {
    LOG(ERROR) << "Couldn't find the slot number corresponding to the "
                  "partition " << boot_device
               << ", number of slots: " << num_slots_
               << ". This device is not updateable.";
    num_slots_ = 1;
    current_slot_ = BootControlInterface::kInvalidSlot;
    return false;
  }

  LOG(INFO) << "Booted from slot " << current_slot_ << " (slot "
            << BootControlInterface::SlotName(current_slot_) << ") of "
            << num_slots_ << " slots present on disk " << boot_disk_name_;
  return true;
}

unsigned int BootControlChromeOS::GetNumSlots() const {
  return num_slots_;
}

BootControlInterface::Slot BootControlChromeOS::GetCurrentSlot() const {
  return current_slot_;
}

bool BootControlChromeOS::GetPartitionDevice(const string& partition_name,
                                             unsigned int slot,
                                             string* device) const {
  int partition_num = GetPartitionNumber(partition_name, slot);
  if (partition_num < 0)
    return false;

  string part_device = utils::MakePartitionName(boot_disk_name_, partition_num);
  if (part_device.empty())
    return false;

  *device = part_device;
  return true;
}

bool BootControlChromeOS::IsSlotBootable(Slot slot) const {
  int partition_num = GetPartitionNumber(kChromeOSPartitionNameKernel, slot);
  if (partition_num < 0)
    return false;

  CgptAddParams params;
  memset(&params, '\0', sizeof(params));
  params.drive_name = const_cast<char*>(boot_disk_name_.c_str());
  params.partition = partition_num;

  int retval = CgptGetPartitionDetails(&params);
  if (retval != CGPT_OK)
    return false;

  return params.successful || params.tries > 0;
}

bool BootControlChromeOS::MarkSlotUnbootable(Slot slot) {
  LOG(INFO) << "Marking slot " << BootControlInterface::SlotName(slot)
            << " unbootable";

  if (slot == current_slot_) {
    LOG(ERROR) << "Refusing to mark current slot as unbootable.";
    return false;
  }

  int partition_num = GetPartitionNumber(kChromeOSPartitionNameKernel, slot);
  if (partition_num < 0)
    return false;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.drive_name = const_cast<char*>(boot_disk_name_.c_str());
  params.partition = partition_num;

  params.successful = false;
  params.set_successful = true;

  params.tries = 0;
  params.set_tries = true;

  int retval = CgptSetAttributes(&params);
  if (retval != CGPT_OK) {
    LOG(ERROR) << "Marking kernel unbootable failed.";
    return false;
  }

  return true;
}

// static
string BootControlChromeOS::SysfsBlockDevice(const string& device) {
  base::FilePath device_path(device);
  if (device_path.DirName().value() != "/dev") {
    return "";
  }
  return base::FilePath("/sys/block").Append(device_path.BaseName()).value();
}

// static
bool BootControlChromeOS::IsRemovableDevice(const string& device) {
  string sysfs_block = SysfsBlockDevice(device);
  string removable;
  if (sysfs_block.empty() ||
      !base::ReadFileToString(base::FilePath(sysfs_block).Append("removable"),
                              &removable)) {
    return false;
  }
  base::TrimWhitespaceASCII(removable, base::TRIM_ALL, &removable);
  return removable == "1";
}

int BootControlChromeOS::GetPartitionNumber(
    const string partition_name,
    BootControlInterface::Slot slot) const {
  if (slot >= num_slots_) {
    LOG(ERROR) << "Invalid slot number: " << slot << ", we only have "
               << num_slots_ << " slot(s)";
    return -1;
  }

  // In Chrome OS, the partition numbers are hard-coded:
  //   KERNEL-A=2, ROOT-A=3, KERNEL-B=4, ROOT-B=4, ...
  // To help compatibility between different we accept both lowercase and
  // uppercase names in the ChromeOS or Brillo standard names.
  // See http://www.chromium.org/chromium-os/chromiumos-design-docs/disk-format
  string partition_lower = base::StringToLowerASCII(partition_name);
  int base_part_num = 2 + 2 * slot;
  if (partition_lower == kChromeOSPartitionNameKernel ||
      partition_lower == kAndroidPartitionNameKernel)
    return base_part_num + 0;
  if (partition_lower == kChromeOSPartitionNameRoot ||
      partition_lower == kAndroidPartitionNameRoot)
    return base_part_num + 1;
  LOG(ERROR) << "Unknown Chrome OS partition name \"" << partition_name << "\"";
  return -1;
}

}  // namespace chromeos_update_engine
