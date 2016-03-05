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
#include <base/strings/string_util.h>

#include "update_engine/common/action_processor.h"
#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/platform_constants.h"
#include "update_engine/common/subprocess.h"
#include "update_engine/common/utils.h"

namespace chromeos_update_engine {

using std::string;
using std::vector;

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
  if (install_plan_.download_url.empty()) {
    LOG(INFO) << "Skipping post-install during rollback";
    return CompletePostinstall(ErrorCode::kSuccess);
  }

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
#ifdef __ANDROID__
  fs_mount_dir_ = "/postinstall";
#else   // __ANDROID__
  TEST_AND_RETURN(
      utils::MakeTempDirectory("au_postint_mount.XXXXXX", &fs_mount_dir_));
#endif  // __ANDROID__

  base::FilePath postinstall_path(partition.postinstall_path);
  if (postinstall_path.IsAbsolute()) {
    LOG(ERROR) << "Invalid absolute path passed to postinstall, use a relative"
                  "path instead: "
               << partition.postinstall_path;
    return CompletePostinstall(ErrorCode::kPostinstallRunnerError);
  }

  string abs_path =
      base::FilePath(fs_mount_dir_).Append(postinstall_path).value();
  if (!base::StartsWith(
          abs_path, fs_mount_dir_, base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "Invalid relative postinstall path: "
               << partition.postinstall_path;
    return CompletePostinstall(ErrorCode::kPostinstallRunnerError);
  }

  if (!utils::MountFilesystem(mountable_device,
                              fs_mount_dir_,
                              MS_RDONLY,
                              partition.filesystem_type,
                              constants::kPostinstallMountOptions)) {
    return CompletePartitionPostinstall(
        1, "Error mounting the device " + mountable_device);
  }

  LOG(INFO) << "Performing postinst (" << partition.postinstall_path << " at "
            << abs_path << ") installed on device " << partition.target_path
            << " and mountable device " << mountable_device;

  // Logs the file format of the postinstall script we are about to run. This
  // will help debug when the postinstall script doesn't match the architecture
  // of our build.
  LOG(INFO) << "Format file for new " << partition.postinstall_path
            << " is: " << utils::GetFileFormat(abs_path);

  // Runs the postinstall script asynchronously to free up the main loop while
  // it's running.
  vector<string> command = {abs_path, partition.target_path};
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
  utils::UnmountFilesystem(fs_mount_dir_);
#ifndef ANDROID
  if (!base::DeleteFile(base::FilePath(fs_mount_dir_), false)) {
    PLOG(WARNING) << "Not removing temporary mountpoint " << fs_mount_dir_;
  }
#endif  // !ANDROID
  fs_mount_dir_.clear();

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
  completer.set_code(error_code);

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
}

}  // namespace chromeos_update_engine
