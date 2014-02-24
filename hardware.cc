// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/hardware.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <rootdev/rootdev.h>
#include <vboot/crossystem.h>

extern "C" {
#include "vboot/vboot_host.h"
}

// We don't use these variables, but libcgpt needs them defined to link.
// TODO(dgarrett) chromium:318536
const char* progname = "";
const char* command = "";
void (*uuid_generator)(uint8_t* buffer) = NULL;

#include "update_engine/hwid_override.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

Hardware::Hardware() {}

Hardware::~Hardware() {}

const string Hardware::BootKernelDevice() {
  return utils::KernelDeviceOfBootDevice(Hardware::BootDevice());
}

const string Hardware::BootDevice() {
  char boot_path[PATH_MAX];
  // Resolve the boot device path fully, including dereferencing
  // through dm-verity.
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

bool Hardware::IsKernelBootable(const std::string& kernel_device,
                                bool* bootable) {
  CgptAddParams params;
  memset(&params, '\0', sizeof(params));

  std::string disk_name;
  int partition_num = 0;

  if (!utils::SplitPartitionName(kernel_device, &disk_name, &partition_num))
    return false;

  params.drive_name = const_cast<char *>(disk_name.c_str());
  params.partition = partition_num;

  int retval = CgptGetPartitionDetails(&params);
  if (retval != CGPT_OK)
    return false;

  *bootable = params.successful || (params.tries > 0);
  return true;
}

std::vector<std::string> Hardware::GetKernelDevices() {
  LOG(INFO) << "GetAllKernelDevices";

  std::string disk_name = utils::GetDiskName(Hardware::BootKernelDevice());
  if(disk_name.empty()) {
    LOG(ERROR) << "Failed to get the cuurent kernel boot disk name";
    return std::vector<std::string>();
  }

  std::vector<std::string> devices;
  const int slot_count = 2; // Use only partition slots A and B
  devices.reserve(slot_count);
  for(int slot = 0; slot < slot_count; slot++) {
    int partition_num = (slot + 1) * 2; // for now, only #2, #4
    std::string device = utils::MakePartitionName(disk_name, partition_num);
    if(!device.empty()) {
      devices.push_back(std::move(device));
    } else {
      LOG(ERROR) << "Cannot make a partition name for disk: "
                 << disk_name << ", partition: " << partition_num;
    }
  }

  return devices;
}


bool Hardware::MarkKernelUnbootable(const std::string& kernel_device) {
  LOG(INFO) << "MarkPartitionUnbootable: " << kernel_device;

  if (kernel_device == BootKernelDevice()) {
    LOG(ERROR) << "Refusing to mark current kernel as unbootable.";
    return false;
  }

  std::string disk_name;
  int partition_num = 0;

  if (!utils::SplitPartitionName(kernel_device, &disk_name, &partition_num))
    return false;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.drive_name = const_cast<char *>(disk_name.c_str());
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

bool Hardware::IsOfficialBuild() {
  return VbGetSystemPropertyInt("debug_build") == 0;
}

bool Hardware::IsNormalBootMode() {
  bool dev_mode = VbGetSystemPropertyInt("devsw_boot") != 0;
  LOG_IF(INFO, dev_mode) << "Booted in dev mode.";
  return !dev_mode;
}

static string ReadValueFromCrosSystem(const string& key) {
  char value_buffer[VB_MAX_STRING_PROPERTY];

  const char *rv = VbGetSystemPropertyString(key.c_str(), value_buffer,
                                             sizeof(value_buffer));
  if (rv != NULL) {
    string return_value(value_buffer);
    TrimWhitespaceASCII(return_value, TRIM_ALL, &return_value);
    return return_value;
  }

  LOG(ERROR) << "Unable to read crossystem key " << key;
  return "";
}

string Hardware::GetHardwareClass() {
  if (USE_HWID_OVERRIDE) {
    return HwidOverride::Read(base::FilePath("/"));
  }
  return ReadValueFromCrosSystem("hwid");
}

string Hardware::GetFirmwareVersion() {
  return ReadValueFromCrosSystem("fwid");
}

string Hardware::GetECVersion() {
  string input_line;
  int exit_code = 0;
  vector<string> cmd(1, "/usr/sbin/mosys");
  cmd.push_back("-k");
  cmd.push_back("ec");
  cmd.push_back("info");

  bool success = Subprocess::SynchronousExec(cmd, &exit_code, &input_line);
  if (!success || exit_code) {
    LOG(ERROR) << "Unable to read ec info from mosys (" << exit_code << ")";
    return "";
  }

  return utils::ParseECVersion(input_line);
}

}  // namespace chromeos_update_engine
