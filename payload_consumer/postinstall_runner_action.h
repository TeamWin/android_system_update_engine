//
// Copyright (C) 2010 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_PAYLOAD_CONSUMER_POSTINSTALL_RUNNER_ACTION_H_
#define UPDATE_ENGINE_PAYLOAD_CONSUMER_POSTINSTALL_RUNNER_ACTION_H_

#include <string>

#include "update_engine/common/action.h"
#include "update_engine/payload_consumer/install_plan.h"

// The Postinstall Runner Action is responsible for running the postinstall
// script of a successfully downloaded update.

namespace chromeos_update_engine {

class BootControlInterface;

class PostinstallRunnerAction : public InstallPlanAction {
 public:
  explicit PostinstallRunnerAction(BootControlInterface* boot_control)
      : PostinstallRunnerAction(boot_control, nullptr) {}

  // InstallPlanAction overrides.
  void PerformAction() override;
  void SuspendAction() override;
  void ResumeAction() override;
  void TerminateProcessing() override;

  // Debugging/logging
  static std::string StaticType() { return "PostinstallRunnerAction"; }
  std::string Type() const override { return StaticType(); }

 private:
  friend class PostinstallRunnerActionTest;

  // Special constructor used for testing purposes.
  PostinstallRunnerAction(BootControlInterface* boot_control,
                          const char* powerwash_marker_file)
      : boot_control_(boot_control),
        powerwash_marker_file_(powerwash_marker_file) {}

  void PerformPartitionPostinstall();

  // Unmount and remove the mountpoint directory if needed.
  void CleanupMount();

  // Subprocess::Exec callback.
  void CompletePartitionPostinstall(int return_code,
                                    const std::string& output);

  // Complete the Action with the passed |error_code| and mark the new slot as
  // ready. Called when the post-install script was run for all the partitions.
  void CompletePostinstall(ErrorCode error_code);

  InstallPlan install_plan_;

  // The path where the filesystem will be mounted during post-install.
  std::string fs_mount_dir_;

  // The partition being processed on the list of partitions specified in the
  // InstallPlan.
  size_t current_partition_{0};

  // The BootControlInerface used to mark the new slot as ready.
  BootControlInterface* boot_control_;

  // True if Powerwash Marker was created before invoking post-install script.
  // False otherwise. Used for cleaning up if post-install fails.
  bool powerwash_marker_created_{false};

  // Non-null value will cause post-install to override the default marker
  // file name; used for testing.
  const char* powerwash_marker_file_;

  // Postinstall command currently running, or 0 if no program running.
  pid_t current_command_{0};

  DISALLOW_COPY_AND_ASSIGN(PostinstallRunnerAction);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_CONSUMER_POSTINSTALL_RUNNER_ACTION_H_
