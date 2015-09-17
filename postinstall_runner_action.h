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

#ifndef UPDATE_ENGINE_POSTINSTALL_RUNNER_ACTION_H_
#define UPDATE_ENGINE_POSTINSTALL_RUNNER_ACTION_H_

#include <string>

#include "update_engine/action.h"
#include "update_engine/install_plan.h"
#include "update_engine/system_state.h"

// The Postinstall Runner Action is responsible for running the postinstall
// script of a successfully downloaded update.

namespace chromeos_update_engine {

class PostinstallRunnerAction : public InstallPlanAction {
 public:
  explicit PostinstallRunnerAction(SystemState* system_state)
      : PostinstallRunnerAction(system_state, nullptr) {}

  void PerformAction();

  // Note that there's no support for terminating this action currently.
  void TerminateProcessing() { CHECK(false); }

  // Debugging/logging
  static std::string StaticType() { return "PostinstallRunnerAction"; }
  std::string Type() const { return StaticType(); }

 private:
  friend class PostinstallRunnerActionTest;

  // Special constructor used for testing purposes.
  PostinstallRunnerAction(SystemState* system_state,
                          const char* powerwash_marker_file)
      : system_state_(system_state),
        powerwash_marker_file_(powerwash_marker_file) {}

  // Subprocess::Exec callback.
  void CompletePostinstall(int return_code,
                           const std::string& output);

  InstallPlan install_plan_;
  std::string temp_rootfs_dir_;

  // The main SystemState singleton.
  SystemState* system_state_;

  // True if Powerwash Marker was created before invoking post-install script.
  // False otherwise. Used for cleaning up if post-install fails.
  bool powerwash_marker_created_ = false;

  // Non-null value will cause post-install to override the default marker
  // file name; used for testing.
  const char* powerwash_marker_file_;

  DISALLOW_COPY_AND_ASSIGN(PostinstallRunnerAction);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_POSTINSTALL_RUNNER_ACTION_H_
