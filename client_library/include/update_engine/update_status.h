//
// Copyright (C) 2015 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_CLIENT_LIBRARY_INCLUDE_UPDATE_ENGINE_UPDATE_STATUS_H_
#define UPDATE_ENGINE_CLIENT_LIBRARY_INCLUDE_UPDATE_ENGINE_UPDATE_STATUS_H_

#include <string>

#include <brillo/enum_flags.h>

// NOTE: Keep this file in sync with
// platform2/system_api/dbus/update_engine/update_engine.proto especially:
// - |UpdateStatus| <-> |Operation|
// - |UpdateEngineStatus| <-> |StatusResult|

namespace update_engine {

// ATTENTION:
// When adding a new enum value:
// - always append at the end with proper adjustments in |ActionCompleted()|.
// - always update |kNonIdleUpdateStatues| in update_attempter_unittest.cc.
// When deprecating an old enum value:
// - other enum values should not change their old values. See b/62842358.
enum class UpdateStatus {
  IDLE = 0,
  CHECKING_FOR_UPDATE = 1,
  UPDATE_AVAILABLE = 2,
  DOWNLOADING = 3,
  VERIFYING = 4,
  FINALIZING = 5,
  UPDATED_NEED_REBOOT = 6,
  REPORTING_ERROR_EVENT = 7,
  ATTEMPTING_ROLLBACK = 8,
  DISABLED = 9,
  // Broadcast this state when an update aborts because user preferences do not
  // allow updates, e.g. over cellular network.
  NEED_PERMISSION_TO_UPDATE = 10,
  CLEANUP_PREVIOUS_UPDATE = 11,

  // This value is exclusively used in Chrome. DO NOT define nor use it.
  // TODO(crbug.com/977320): Remove this value from chrome by refactoring the
  // Chrome code and evantually from here. This is not really an operation or
  // state that the update_engine stays on. This is the result of an internal
  // failure and should be reflected differently.
  // ERROR = -1,
};

// Enum of bit-wise flags for controlling how updates are attempted.
enum UpdateAttemptFlags : int32_t {
  kNone = 0,
  // Treat the update like a non-interactive update, even when being triggered
  // by the interactive APIs.
  kFlagNonInteractive = (1 << 0),
  // Restrict (disallow) downloading of updates.
  kFlagRestrictDownload = (1 << 1),
};

// Enable bit-wise operators for the above enumeration of flag values.
DECLARE_FLAGS_ENUM(UpdateAttemptFlags);

struct UpdateEngineStatus {
  // Update engine last checked update (time_t: seconds from unix epoch).
  int64_t last_checked_time;
  // Current status/operation of the update_engine.
  UpdateStatus status;
  // Current product version (oem bundle id).
  std::string current_version;
  // Current progress (0.0f-1.0f).
  double progress;
  // Size of the update in bytes.
  uint64_t new_size_bytes;
  // New product version.
  std::string new_version;
  // Wether the update is an enterprise rollback. The value is valid only if the
  // current operation is passed CHECKING_FOR_UPDATE.
  bool is_enterprise_rollback;
  // Indication of install for DLC(s).
  bool is_install;
  // The end-of-life date of the device in the number of days since Unix Epoch.
  int64_t eol_date;
  // The system will powerwash once the update is applied.
  bool will_powerwash_after_reboot;
};

}  // namespace update_engine

#endif  // UPDATE_ENGINE_CLIENT_LIBRARY_INCLUDE_UPDATE_ENGINE_UPDATE_STATUS_H_
