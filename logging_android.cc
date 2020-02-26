//
// Copyright (C) 2020 The Android Open Source Project
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

#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include <base/files/dir_reader_posix.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "update_engine/common/utils.h"

using std::string;

namespace chromeos_update_engine {
namespace {

constexpr char kSystemLogsRoot[] = "/data/misc/update_engine_log";
constexpr size_t kLogCount = 5;

// Keep the most recent |kLogCount| logs but remove the old ones in
// "/data/misc/update_engine_log/".
void DeleteOldLogs(const string& kLogsRoot) {
  base::DirReaderPosix reader(kLogsRoot.c_str());
  if (!reader.IsValid()) {
    LOG(ERROR) << "Failed to read " << kLogsRoot;
    return;
  }

  std::vector<string> old_logs;
  while (reader.Next()) {
    if (reader.name()[0] == '.')
      continue;

    // Log files are in format "update_engine.%Y%m%d-%H%M%S",
    // e.g. update_engine.20090103-231425
    uint64_t date;
    uint64_t local_time;
    if (sscanf(reader.name(),
               "update_engine.%" PRIu64 "-%" PRIu64 "",
               &date,
               &local_time) == 2) {
      old_logs.push_back(reader.name());
    } else {
      LOG(WARNING) << "Unrecognized log file " << reader.name();
    }
  }

  std::sort(old_logs.begin(), old_logs.end(), std::greater<string>());
  for (size_t i = kLogCount; i < old_logs.size(); i++) {
    string log_path = kLogsRoot + "/" + old_logs[i];
    if (unlink(log_path.c_str()) == -1) {
      PLOG(WARNING) << "Failed to unlink " << log_path;
    }
  }
}

string SetupLogFile(const string& kLogsRoot) {
  DeleteOldLogs(kLogsRoot);

  return base::StringPrintf("%s/update_engine.%s",
                            kLogsRoot.c_str(),
                            utils::GetTimeAsString(::time(nullptr)).c_str());
}

}  // namespace

void SetupLogging(bool log_to_system, bool log_to_file) {
  logging::LoggingSettings log_settings;
  log_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  log_settings.logging_dest = static_cast<logging::LoggingDestination>(
      (log_to_system ? logging::LOG_TO_SYSTEM_DEBUG_LOG : 0) |
      (log_to_file ? logging::LOG_TO_FILE : 0));
  log_settings.log_file = nullptr;

  string log_file;
  if (log_to_file) {
    log_file = SetupLogFile(kSystemLogsRoot);
    log_settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;
    log_settings.log_file = log_file.c_str();
  }
  logging::InitLogging(log_settings);

  // The log file will have AID_LOG as group ID; this GID is inherited from the
  // parent directory "/data/misc/update_engine_log" which sets the SGID bit.
  chmod(log_file.c_str(), 0640);
}

}  // namespace chromeos_update_engine
