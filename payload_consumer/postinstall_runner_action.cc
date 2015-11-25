//
// Copyright (C) 2011 The Android Open Source Project
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

#include "update_engine/payload_consumer/postinstall_runner_action.h"

#include <stdlib.h>
#include <sys/mount.h>
#include <vector>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>

#include "update_engine/common/action_processor.h"
#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/subprocess.h"
#include "update_engine/common/utils.h"

namespace chromeos_update_engine {

using std::string;
using std::vector;

namespace {
// The absolute path to the post install command.
const char kPostinstallScript[] = "/postinst";

// Path to the binary file used by kPostinstallScript. Used to get and log the
// file format of the binary to debug issues when the ELF format on the update
// doesn't match the one on the current system. This path is not executed.
const char kDebugPostinstallBinaryPath[] = "/usr/bin/cros_installer";
}

void PostinstallRunnerAction::PerformAction() {
  CHECK(HasInputObject());
  install_plan_ = GetInputObject();

  if (install_plan_.powerwash_required) {
    if (utils::CreatePowerwashMarkerFile(powerwash_marker_file_)) {
      powerwash_marker_created_ = true;
    } else {
      return CompletePostinstall(ErrorCode::kPostinstallPowerwashError);
    }
  }

  PerformPartitionPostinstall();
}

void PostinstallRunnerAction::PerformPartitionPostinstall() {
  // Skip all the partitions that don't have a post-install step.
  while (current_partition_ < install_plan_.partitions.size() &&
         !install_plan_.partitions[current_partition_].run_postinstall) {
    VLOG(1) << "Skipping post-install on partition "
            << install_plan_.partitions[current_partition_].name;
    current_partition_++;
  }
  if (current_partition_ == install_plan_.partitions.size())
    return CompletePostinstall(ErrorCode::kSuccess);

  const InstallPlan::Partition& partition =
      install_plan_.partitions[current_partition_];

  const string mountable_device =
      utils::MakePartitionNameForMount(partition.target_path);
  if (mountable_device.empty()) {
    LOG(ERROR) << "Cannot make mountable device from " << partition.target_path;
    return CompletePostinstall(ErrorCode::kPostinstallRunnerError);
  }

  // Perform post-install for the current_partition_ partition. At this point we
  // need to call CompletePartitionPostinstall to complete the operation and
  // cleanup.
  TEST_AND_RETURN(
      utils::MakeTempDirectory("au_postint_mount.XXXXXX", &temp_rootfs_dir_));

  if (!utils::MountFilesystem(mountable_device, temp_rootfs_dir_, MS_RDONLY)) {
    return CompletePartitionPostinstall(
        1, "Error mounting the device " + mountable_device);
  }

  LOG(INFO) << "Performing postinst (" << kPostinstallScript
            << ") installed on device " << partition.target_path
            << " and mountable device " << mountable_device;

  // Logs the file format of the postinstall script we are about to run. This
  // will help debug when the postinstall script doesn't match the architecture
  // of our build.
  LOG(INFO) << "Format file for new " <<  kPostinstallScript << " is: "
            << utils::GetFileFormat(temp_rootfs_dir_ + kPostinstallScript);
  LOG(INFO) << "Format file for new " <<  kDebugPostinstallBinaryPath << " is: "
            << utils::GetFileFormat(
                temp_rootfs_dir_ + kDebugPostinstallBinaryPath);

  // Runs the postinstall script asynchronously to free up the main loop while
  // it's running.
  vector<string> command;
  if (!install_plan_.download_url.empty()) {
    command.push_back(temp_rootfs_dir_ + kPostinstallScript);
  } else {
    // TODO(sosa): crbug.com/366207.
    // If we're doing a rollback, just run our own postinstall.
    command.push_back(kPostinstallScript);
  }
  command.push_back(partition.target_path);
  if (!Subprocess::Get().Exec(
          command,
          base::Bind(
              &PostinstallRunnerAction::CompletePartitionPostinstall,
              base::Unretained(this)))) {
    CompletePartitionPostinstall(1, "Postinstall didn't launch");
  }
}

void PostinstallRunnerAction::CompletePartitionPostinstall(
    int return_code,
    const string& output) {
  utils::UnmountFilesystem(temp_rootfs_dir_);
  if (!base::DeleteFile(base::FilePath(temp_rootfs_dir_), false)) {
    PLOG(WARNING) << "Not removing mountpoint " << temp_rootfs_dir_;
  }
  temp_rootfs_dir_.clear();

  if (return_code != 0) {
    LOG(ERROR) << "Postinst command failed with code: " << return_code;
    ErrorCode error_code = ErrorCode::kPostinstallRunnerError;

    if (return_code == 3) {
      // This special return code means that we tried to update firmware,
      // but couldn't because we booted from FW B, and we need to reboot
      // to get back to FW A.
      error_code = ErrorCode::kPostinstallBootedFromFirmwareB;
    }

    if (return_code == 4) {
      // This special return code means that we tried to update firmware,
      // but couldn't because we booted from FW B, and we need to reboot
      // to get back to FW A.
      error_code = ErrorCode::kPostinstallFirmwareRONotUpdatable;
    }
    return CompletePostinstall(error_code);
  }
  current_partition_++;
  PerformPartitionPostinstall();
}

void PostinstallRunnerAction::CompletePostinstall(ErrorCode error_code) {
  // We only attempt to mark the new slot as active if all the postinstall
  // steps succeeded.
  if (error_code == ErrorCode::kSuccess &&
      !boot_control_->SetActiveBootSlot(install_plan_.target_slot)) {
    error_code = ErrorCode::kPostinstallRunnerError;
  }

  ScopedActionCompleter completer(processor_, this);

  if (error_code != ErrorCode::kSuccess) {
    LOG(ERROR) << "Postinstall action failed.";

    // Undo any changes done to trigger Powerwash using clobber-state.
    if (powerwash_marker_created_)
      utils::DeletePowerwashMarkerFile(powerwash_marker_file_);

    return;
  }

  LOG(INFO) << "All post-install commands succeeded";
  if (HasOutputPipe()) {
    SetOutputObject(install_plan_);
  }

  completer.set_code(ErrorCode::kSuccess);
}

}  // namespace chromeos_update_engine
