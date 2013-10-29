// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/hardware.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <rootdev/rootdev.h>

#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

static const char kDevImageMarker[] = "/root/.dev_mode";


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

bool Hardware::IsOfficialBuild() {
  return !file_util::PathExists(FilePath(kDevImageMarker));
}

bool Hardware::IsNormalBootMode() {
  // TODO(petkov): Convert to a library call once a crossystem library is
  // available (crosbug.com/13291).
  int exit_code = 0;
  vector<string> cmd(1, "/usr/bin/crossystem");
  cmd.push_back("devsw_boot?1");

  // Assume dev mode if the dev switch is set to 1 and there was no error
  // executing crossystem. Assume normal mode otherwise.
  bool success = Subprocess::SynchronousExec(cmd, &exit_code, NULL);
  bool dev_mode = success && exit_code == 0;
  LOG_IF(INFO, dev_mode) << "Booted in dev mode.";
  return !dev_mode;
}

static string ReadValueFromCrosSystem(const string& key) {
  int exit_code = 0;
  vector<string> cmd(1, "/usr/bin/crossystem");
  cmd.push_back(key);

  string return_value;
  bool success = Subprocess::SynchronousExec(cmd, &exit_code, &return_value);
  if (success && !exit_code) {
    TrimWhitespaceASCII(return_value, TRIM_ALL, &return_value);
    return return_value;
  }
  LOG(ERROR) << "Unable to read " << key << " (" << exit_code << ") "
             << return_value;
  return "";
}

string Hardware::GetHardwareClass() {
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
