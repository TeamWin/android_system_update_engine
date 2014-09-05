// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/hardware.h"

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <rootdev/rootdev.h>
#include <vboot/crossystem.h>

extern "C" {
#include "vboot/vboot_host.h"
}

#include "update_engine/hwid_override.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace {

static const char kOOBECompletedMarker[] = "/home/chronos/.oobe_completed";

}  // namespace

namespace chromeos_update_engine {

Hardware::Hardware() {}

Hardware::~Hardware() {}

string Hardware::BootKernelDevice() const {
  return utils::KernelDeviceOfBootDevice(Hardware::BootDevice());
}

string Hardware::BootDevice() const {
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

bool Hardware::IsBootDeviceRemovable() const {
  return utils::IsRemovableDevice(utils::GetDiskName(BootDevice()));
}

bool Hardware::IsKernelBootable(const std::string& kernel_device,
                                bool* bootable) const {
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

std::vector<std::string> Hardware::GetKernelDevices() const {
  LOG(INFO) << "GetAllKernelDevices";

  std::string disk_name = utils::GetDiskName(Hardware::BootKernelDevice());
  if (disk_name.empty()) {
    LOG(ERROR) << "Failed to get the current kernel boot disk name";
    return std::vector<std::string>();
  }

  std::vector<std::string> devices;
  for (int partition_num : {2, 4}) {  // for now, only #2, #4 for slot A & B
    std::string device = utils::MakePartitionName(disk_name, partition_num);
    if (!device.empty()) {
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

bool Hardware::IsOfficialBuild() const {
  return VbGetSystemPropertyInt("debug_build") == 0;
}

bool Hardware::IsNormalBootMode() const {
  bool dev_mode = VbGetSystemPropertyInt("devsw_boot") != 0;
  LOG_IF(INFO, dev_mode) << "Booted in dev mode.";
  return !dev_mode;
}

bool Hardware::IsOOBEComplete(base::Time* out_time_of_oobe) const {
  struct stat statbuf;
  if (stat(kOOBECompletedMarker, &statbuf) != 0) {
    if (errno != ENOENT) {
      PLOG(ERROR) << "Error getting information about "
                  << kOOBECompletedMarker;
    }
    return false;
  }

  if (out_time_of_oobe != nullptr)
    *out_time_of_oobe = base::Time::FromTimeT(statbuf.st_mtime);
  return true;
}

static string ReadValueFromCrosSystem(const string& key) {
  char value_buffer[VB_MAX_STRING_PROPERTY];

  const char *rv = VbGetSystemPropertyString(key.c_str(), value_buffer,
                                             sizeof(value_buffer));
  if (rv != nullptr) {
    string return_value(value_buffer);
    base::TrimWhitespaceASCII(return_value, base::TRIM_ALL, &return_value);
    return return_value;
  }

  LOG(ERROR) << "Unable to read crossystem key " << key;
  return "";
}

string Hardware::GetHardwareClass() const {
  if (USE_HWID_OVERRIDE) {
    return HwidOverride::Read(base::FilePath("/"));
  }
  return ReadValueFromCrosSystem("hwid");
}

string Hardware::GetFirmwareVersion() const {
  return ReadValueFromCrosSystem("fwid");
}

string Hardware::GetECVersion() const {
  string input_line;
  int exit_code = 0;
  vector<string> cmd = {"/usr/sbin/mosys", "-k", "ec", "info"};

  bool success = Subprocess::SynchronousExec(cmd, &exit_code, &input_line);
  if (!success || exit_code) {
    LOG(ERROR) << "Unable to read ec info from mosys (" << exit_code << ")";
    return "";
  }

  return utils::ParseECVersion(input_line);
}

}  // namespace chromeos_update_engine
