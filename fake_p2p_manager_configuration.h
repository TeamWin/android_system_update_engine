// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_FAKE_P2P_MANAGER_CONFIGURATION_H_
#define UPDATE_ENGINE_FAKE_P2P_MANAGER_CONFIGURATION_H_

#include "update_engine/p2p_manager.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/string_number_conversions.h>

namespace chromeos_update_engine {

// Configuration for P2PManager for use in unit tests. Instead of
// /var/cache/p2p, a temporary directory is used.
class FakeP2PManagerConfiguration : public P2PManager::Configuration {
 public:
  FakeP2PManagerConfiguration() {
    EXPECT_TRUE(utils::MakeTempDirectory("/tmp/p2p-tc.XXXXXX", &p2p_dir_));
  }

  ~FakeP2PManagerConfiguration() {
    if (p2p_dir_.size() > 0 && !test_utils::RecursiveUnlinkDir(p2p_dir_)) {
      PLOG(ERROR) << "Unable to unlink files and directory in " << p2p_dir_;
    }
  }

  // P2PManager::Configuration override
  base::FilePath GetP2PDir() override {
    return base::FilePath(p2p_dir_);
  }

  // P2PManager::Configuration override
  std::vector<std::string> GetInitctlArgs(bool is_start) override {
    return is_start ? initctl_start_args_ : initctl_stop_args_;
  }

  // P2PManager::Configuration override
  std::vector<std::string> GetP2PClientArgs(const std::string &file_id,
                                            size_t minimum_size) override {
    std::vector<std::string> formatted_command = p2p_client_cmd_format_;
    // Replace {variable} on the passed string.
    std::string str_minimum_size = base::SizeTToString(minimum_size);
    for (std::string& arg : formatted_command) {
      ReplaceSubstringsAfterOffset(&arg, 0, "{file_id}", file_id);
      ReplaceSubstringsAfterOffset(&arg, 0, "{minsize}", str_minimum_size);
    }
    return formatted_command;
  }

  // Use |command_line| instead of "initctl start p2p" when attempting
  // to start the p2p service.
  void SetInitctlStartCommand(const std::vector<std::string>& command) {
    initctl_start_args_ = command;
  }

  // Use |command_line| instead of "initctl stop p2p" when attempting
  // to stop the p2p service.
  void SetInitctlStopCommand(const std::vector<std::string>& command) {
    initctl_stop_args_ = command;
  }

  // Use |command_format| instead of "p2p-client --get-url={file_id}
  // --minimum-size={minsize}" when attempting to look up a file using
  // p2p-client(1).
  //
  // The passed |command_format| argument can have "{file_id}" and "{minsize}"
  // as substrings of any of its elements, that will be replaced by the
  // corresponding values passed to GetP2PClientArgs().
  void SetP2PClientCommand(const std::vector<std::string>& command_format) {
    p2p_client_cmd_format_ = command_format;
  }

 private:
  // The temporary directory used for p2p.
  std::string p2p_dir_;

  // Argument vector for starting p2p.
  std::vector<std::string> initctl_start_args_{"initctl", "start", "p2p"};

  // Argument vector for stopping p2p.
  std::vector<std::string> initctl_stop_args_{"initctl", "stop", "p2p"};

  // A string for generating the p2p-client command. See the
  // SetP2PClientCommandLine() for details.
  std::vector<std::string> p2p_client_cmd_format_{
      "p2p-client", "--get-url={file_id}", "--minimum-size={minsize}"};

  DISALLOW_COPY_AND_ASSIGN(FakeP2PManagerConfiguration);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_FAKE_P2P_MANAGER_CONFIGURATION_H_
