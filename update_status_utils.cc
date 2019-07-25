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
#include "update_engine/update_status_utils.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/key_value_store.h>
#include <update_engine/dbus-constants.h>

using brillo::KeyValueStore;
using std::string;
using update_engine::UpdateEngineStatus;
using update_engine::UpdateStatus;

namespace chromeos_update_engine {

const char* UpdateStatusToString(const UpdateStatus& status) {
  switch (status) {
    case UpdateStatus::IDLE:
      return update_engine::kUpdateStatusIdle;
    case UpdateStatus::CHECKING_FOR_UPDATE:
      return update_engine::kUpdateStatusCheckingForUpdate;
    case UpdateStatus::UPDATE_AVAILABLE:
      return update_engine::kUpdateStatusUpdateAvailable;
    case UpdateStatus::NEED_PERMISSION_TO_UPDATE:
      return update_engine::kUpdateStatusNeedPermissionToUpdate;
    case UpdateStatus::DOWNLOADING:
      return update_engine::kUpdateStatusDownloading;
    case UpdateStatus::VERIFYING:
      return update_engine::kUpdateStatusVerifying;
    case UpdateStatus::FINALIZING:
      return update_engine::kUpdateStatusFinalizing;
    case UpdateStatus::UPDATED_NEED_REBOOT:
      return update_engine::kUpdateStatusUpdatedNeedReboot;
    case UpdateStatus::REPORTING_ERROR_EVENT:
      return update_engine::kUpdateStatusReportingErrorEvent;
    case UpdateStatus::ATTEMPTING_ROLLBACK:
      return update_engine::kUpdateStatusAttemptingRollback;
    case UpdateStatus::DISABLED:
      return update_engine::kUpdateStatusDisabled;
  }

  NOTREACHED();
  return nullptr;
}

string UpdateEngineStatusToString(const UpdateEngineStatus& status) {
  KeyValueStore key_value_store;

#if BASE_VER < 576279
  key_value_store.SetString("LAST_CHECKED_TIME",
                            base::Int64ToString(status.last_checked_time));
  key_value_store.SetString("PROGRESS", base::DoubleToString(status.progress));
  key_value_store.SetString("NEW_SIZE",
                            base::Uint64ToString(status.new_size_bytes));
#else
  key_value_store.SetString("LAST_CHECKED_TIME",
                            base::NumberToString(status.last_checked_time));
  key_value_store.SetString("PROGRESS", base::NumberToString(status.progress));
  key_value_store.SetString("NEW_SIZE",
                            base::NumberToString(status.new_size_bytes));
#endif
  key_value_store.SetString("CURRENT_OPERATION",
                            UpdateStatusToString(status.status));
  key_value_store.SetString("NEW_VERSION", status.new_version);
  key_value_store.SetBoolean("IS_INSTALL", status.is_install);

  return key_value_store.SaveToString();
}

}  // namespace chromeos_update_engine
