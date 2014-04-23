// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/postinstall_runner_action.h"

#include <sys/mount.h>
#include <stdlib.h>
#include <vector>

#include "update_engine/action_processor.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

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
  const string install_device = install_plan_.install_path;
  ScopedActionCompleter completer(processor_, this);

  // Make mountpoint.
  TEST_AND_RETURN(utils::MakeTempDirectory("/tmp/au_postint_mount.XXXXXX",
                                           &temp_rootfs_dir_));
  ScopedDirRemover temp_dir_remover(temp_rootfs_dir_);

  unsigned long mountflags = MS_RDONLY;
  int rc = mount(install_device.c_str(),
                 temp_rootfs_dir_.c_str(),
                 "ext2",
                 mountflags,
                 NULL);
  // TODO(sosa): Remove once crbug.com/208022 is resolved.
  if (rc < 0) {
    LOG(INFO) << "Failed to mount install part "
              << install_device << " as ext2. Trying ext3.";
    rc = mount(install_device.c_str(),
               temp_rootfs_dir_.c_str(),
               "ext3",
               mountflags,
               NULL);
  }
  if (rc < 0) {
    LOG(ERROR) << "Unable to mount destination device " << install_device
               << " onto " << temp_rootfs_dir_;
    return;
  }

  temp_dir_remover.set_should_remove(false);
  completer.set_should_complete(false);

  if (install_plan_.powerwash_required) {
    if (utils::CreatePowerwashMarkerFile(powerwash_marker_file_)) {
      powerwash_marker_created_ = true;
    } else {
      completer.set_code(kErrorCodePostinstallPowerwashError);
      return;
    }
  }

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
  command.push_back(install_device);
  if (!Subprocess::Get().Exec(command, StaticCompletePostinstall, this)) {
    CompletePostinstall(1);
  }
}

void PostinstallRunnerAction::CompletePostinstall(int return_code) {
  ScopedActionCompleter completer(processor_, this);
  ScopedTempUnmounter temp_unmounter(temp_rootfs_dir_);
  if (return_code != 0) {
    LOG(ERROR) << "Postinst command failed with code: " << return_code;

    // Undo any changes done to trigger Powerwash using clobber-state.
    if (powerwash_marker_created_)
      utils::DeletePowerwashMarkerFile(powerwash_marker_file_);

    if (return_code == 3) {
      // This special return code means that we tried to update firmware,
      // but couldn't because we booted from FW B, and we need to reboot
      // to get back to FW A.
      completer.set_code(kErrorCodePostinstallBootedFromFirmwareB);
    }

    if (return_code == 4) {
      // This special return code means that we tried to update firmware,
      // but couldn't because we booted from FW B, and we need to reboot
      // to get back to FW A.
      completer.set_code(kErrorCodePostinstallFirmwareRONotUpdatable);
    }

    return;
  }

  LOG(INFO) << "Postinst command succeeded";
  if (HasOutputPipe()) {
    SetOutputObject(install_plan_);
  }

  completer.set_code(kErrorCodeSuccess);
}

void PostinstallRunnerAction::StaticCompletePostinstall(int return_code,
                                                        const string& output,
                                                        void* p) {
  reinterpret_cast<PostinstallRunnerAction*>(p)->CompletePostinstall(
      return_code);
}

}  // namespace chromeos_update_engine
