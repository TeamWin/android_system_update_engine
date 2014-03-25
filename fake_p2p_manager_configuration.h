// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_P2P_MANAGER_CONFIGURATION_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_P2P_MANAGER_CONFIGURATION_H_

#include "update_engine/p2p_manager.h"
#include "update_engine/utils.h"

#include <glib.h>

#include <base/logging.h>
#include <base/strings/stringprintf.h>

namespace chromeos_update_engine {

// Configuration for P2PManager for use in unit tests. Instead of
// /var/cache/p2p, a temporary directory is used.
class FakeP2PManagerConfiguration : public P2PManager::Configuration {
public:
  FakeP2PManagerConfiguration()
    : p2p_client_cmdline_format_("p2p-client --get-url=%s --minimum-size=%zu") {
    EXPECT_TRUE(utils::MakeTempDirectory("/tmp/p2p-tc.XXXXXX", &p2p_dir_));
    SetInitctlStartCommandLine("initctl start p2p");
    SetInitctlStopCommandLine("initctl stop p2p");
  }

  ~FakeP2PManagerConfiguration() {
    if (p2p_dir_.size() > 0 && !utils::RecursiveUnlinkDir(p2p_dir_)) {
      PLOG(ERROR) << "Unable to unlink files and directory in " << p2p_dir_;
    }
  }

  // P2PManager::Configuration override
  virtual base::FilePath GetP2PDir() {
    return base::FilePath(p2p_dir_);
  };

  // P2PManager::Configuration override
  virtual std::vector<std::string> GetInitctlArgs(bool is_start) {
    return is_start ? initctl_start_args_ : initctl_stop_args_;
  }

  // P2PManager::Configuration override
  virtual std::vector<std::string> GetP2PClientArgs(const std::string &file_id,
                                                    size_t minimum_size) {
    std::string formatted_command_line =
        base::StringPrintf(p2p_client_cmdline_format_.c_str(),
                           file_id.c_str(), minimum_size);
    return ParseCommandLine(formatted_command_line);
  }

  // Use |command_line| instead of "initctl start p2p" when attempting
  // to start the p2p service.
  void SetInitctlStartCommandLine(const std::string &command_line) {
    initctl_start_args_ = ParseCommandLine(command_line);
  }

  // Use |command_line| instead of "initctl stop p2p" when attempting
  // to stop the p2p service.
  void SetInitctlStopCommandLine(const std::string &command_line) {
    initctl_stop_args_ = ParseCommandLine(command_line);
  }

  // Use |command_line_format| instead of "p2p-client --get-url=%s
  // --minimum-size=%zu" when attempting to look up a file using
  // p2p-client(1).
  //
  // The passed |command_line_format| argument should be a
  // printf()-style format string taking two arguments, the first
  // being the a C string for the p2p file id (e.g. %s) and the second
  // being a size_t with the minimum_size.
  void SetP2PClientCommandLine(const std::string &command_line_format) {
    p2p_client_cmdline_format_ = command_line_format;
  }

private:
  // Helper for parsing and splitting |command_line| into an argument
  // vector in much the same way a shell would except for not
  // supporting wildcards, globs, operators etc. See
  // g_shell_parse_argv() for more details. If an error occurs, the
  // empty vector is returned.
  std::vector<std::string> ParseCommandLine(const std::string &command_line) {
    gint argc;
    gchar **argv;
    std::vector<std::string> ret;

    if (!g_shell_parse_argv(command_line.c_str(),
                            &argc,
                            &argv,
                            NULL)) {
      LOG(ERROR) << "Error splitting '" << command_line << "'";
      return ret;
    }
    for (int n = 0; n < argc; n++)
      ret.push_back(argv[n]);
    g_strfreev(argv);
    return ret;
  }

  // The temporary directory used for p2p.
  std::string p2p_dir_;

  // Argument vector for starting p2p.
  std::vector<std::string> initctl_start_args_;

  // Argument vector for stopping p2p.
  std::vector<std::string> initctl_stop_args_;

  // A printf()-style format string for generating the p2p-client format.
  // See the SetP2PClientCommandLine() for details.
  std::string p2p_client_cmdline_format_;

  DISALLOW_COPY_AND_ASSIGN(FakeP2PManagerConfiguration);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_P2P_MANAGER_CONFIGURATION_H_
