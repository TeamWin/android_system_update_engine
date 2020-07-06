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

#include "update_engine/client_library/client_dbus.h"

#include <base/message_loop/message_loop.h>

#include <memory>

#include <dbus/bus.h>
#include <update_engine/dbus-constants.h>

#include "update_engine/update_status_utils.h"

using dbus::Bus;
using org::chromium::UpdateEngineInterfaceProxy;
using std::string;
using std::unique_ptr;
using std::vector;

namespace update_engine {

unique_ptr<UpdateEngineClient> UpdateEngineClient::CreateInstance() {
  auto ret = std::make_unique<internal::DBusUpdateEngineClient>();
  if (!ret->Init()) {
    ret.reset();
  }
  return ret;
}

namespace internal {

namespace {
// This converts the status from Protobuf |StatusResult| to The internal
// |UpdateEngineStatus| struct.
void ConvertToUpdateEngineStatus(const StatusResult& status,
                                 UpdateEngineStatus* out_status) {
  out_status->last_checked_time = status.last_checked_time();
  out_status->progress = status.progress();
  out_status->new_version = status.new_version();
  out_status->new_size_bytes = status.new_size();
  out_status->status = static_cast<UpdateStatus>(status.current_operation());
  out_status->is_enterprise_rollback = status.is_enterprise_rollback();
  out_status->is_install = status.is_install();
  out_status->eol_date = status.eol_date();
  out_status->will_powerwash_after_reboot =
      status.will_powerwash_after_reboot();
}
}  // namespace

bool DBusUpdateEngineClient::Init() {
  Bus::Options options;
  options.bus_type = Bus::SYSTEM;
  scoped_refptr<Bus> bus{new Bus{options}};

  if (!bus->Connect())
    return false;

  proxy_.reset(new UpdateEngineInterfaceProxy{bus});
  return true;
}

bool DBusUpdateEngineClient::AttemptUpdate(const string& in_app_version,
                                           const string& in_omaha_url,
                                           bool at_user_request) {
  return proxy_->AttemptUpdateWithFlags(
      in_app_version,
      in_omaha_url,
      (at_user_request)
          ? 0
          : update_engine::UpdateAttemptFlags::kFlagNonInteractive,
      nullptr);
}

bool DBusUpdateEngineClient::AttemptInstall(const string& omaha_url,
                                            const vector<string>& dlc_ids) {
  return proxy_->AttemptInstall(omaha_url, dlc_ids, nullptr);
}

bool DBusUpdateEngineClient::SetDlcActiveValue(bool is_active,
                                               const std::string& dlc_id) {
  return proxy_->SetDlcActiveValue(is_active, dlc_id, /*error=*/nullptr);
}

bool DBusUpdateEngineClient::GetStatus(UpdateEngineStatus* out_status) const {
  StatusResult status;
  if (!proxy_->GetStatusAdvanced(&status, nullptr)) {
    return false;
  }

  ConvertToUpdateEngineStatus(status, out_status);
  return true;
}

bool DBusUpdateEngineClient::SetCohortHint(const string& cohort_hint) {
  return proxy_->SetCohortHint(cohort_hint, nullptr);
}

bool DBusUpdateEngineClient::GetCohortHint(string* cohort_hint) const {
  return proxy_->GetCohortHint(cohort_hint, nullptr);
}

bool DBusUpdateEngineClient::SetUpdateOverCellularPermission(bool allowed) {
  return proxy_->SetUpdateOverCellularPermission(allowed, nullptr);
}

bool DBusUpdateEngineClient::GetUpdateOverCellularPermission(
    bool* allowed) const {
  return proxy_->GetUpdateOverCellularPermission(allowed, nullptr);
}

bool DBusUpdateEngineClient::SetP2PUpdatePermission(bool enabled) {
  return proxy_->SetP2PUpdatePermission(enabled, nullptr);
}

bool DBusUpdateEngineClient::GetP2PUpdatePermission(bool* enabled) const {
  return proxy_->GetP2PUpdatePermission(enabled, nullptr);
}

bool DBusUpdateEngineClient::Rollback(bool powerwash) {
  return proxy_->AttemptRollback(powerwash, nullptr);
}

bool DBusUpdateEngineClient::GetRollbackPartition(
    string* rollback_partition) const {
  return proxy_->GetRollbackPartition(rollback_partition, nullptr);
}

bool DBusUpdateEngineClient::GetPrevVersion(string* prev_version) const {
  return proxy_->GetPrevVersion(prev_version, nullptr);
}

void DBusUpdateEngineClient::RebootIfNeeded() {
  bool ret = proxy_->RebootIfNeeded(nullptr);
  if (!ret) {
    // Reboot error code doesn't necessarily mean that a reboot
    // failed. For example, D-Bus may be shutdown before we receive the
    // result.
    LOG(INFO) << "RebootIfNeeded() failure ignored.";
  }
}

bool DBusUpdateEngineClient::ResetStatus() {
  return proxy_->ResetStatus(nullptr);
}

void DBusUpdateEngineClient::DBusStatusHandlersRegistered(
    const string& interface, const string& signal_name, bool success) const {
  if (!success) {
    for (auto handler : handlers_) {
      handler->IPCError("Could not connect to" + signal_name + " on " +
                        interface);
    }
  } else {
    StatusUpdateHandlersRegistered(nullptr);
  }
}

void DBusUpdateEngineClient::StatusUpdateHandlersRegistered(
    StatusUpdateHandler* handler) const {
  UpdateEngineStatus status;
  if (!GetStatus(&status)) {
    handler->IPCError("Could not query current status");
    return;
  }

  std::vector<update_engine::StatusUpdateHandler*> just_handler = {handler};
  for (auto h : handler ? just_handler : handlers_) {
    h->HandleStatusUpdate(status);
  }
}

void DBusUpdateEngineClient::RunStatusUpdateHandlers(
    const StatusResult& status) {
  UpdateEngineStatus ue_status;
  ConvertToUpdateEngineStatus(status, &ue_status);

  for (auto handler : handlers_) {
    handler->HandleStatusUpdate(ue_status);
  }
}

bool DBusUpdateEngineClient::UnregisterStatusUpdateHandler(
    StatusUpdateHandler* handler) {
  auto it = std::find(handlers_.begin(), handlers_.end(), handler);
  if (it != handlers_.end()) {
    handlers_.erase(it);
    return true;
  }

  return false;
}

bool DBusUpdateEngineClient::RegisterStatusUpdateHandler(
    StatusUpdateHandler* handler) {
  if (!base::MessageLoopCurrent::IsSet()) {
    LOG(FATAL) << "Cannot get UpdateEngineClient outside of message loop.";
    return false;
  }

  handlers_.push_back(handler);

  if (dbus_handler_registered_) {
    StatusUpdateHandlersRegistered(handler);
    return true;
  }

  proxy_->RegisterStatusUpdateAdvancedSignalHandler(
      base::Bind(&DBusUpdateEngineClient::RunStatusUpdateHandlers,
                 base::Unretained(this)),
      base::Bind(&DBusUpdateEngineClient::DBusStatusHandlersRegistered,
                 base::Unretained(this)));

  dbus_handler_registered_ = true;

  return true;
}

bool DBusUpdateEngineClient::SetTargetChannel(const string& in_target_channel,
                                              bool allow_powerwash) {
  return proxy_->SetChannel(in_target_channel, allow_powerwash, nullptr);
}

bool DBusUpdateEngineClient::GetTargetChannel(string* out_channel) const {
  return proxy_->GetChannel(false,  // Get the target channel.
                            out_channel,
                            nullptr);
}

bool DBusUpdateEngineClient::GetChannel(string* out_channel) const {
  return proxy_->GetChannel(true,  // Get the current channel.
                            out_channel,
                            nullptr);
}

bool DBusUpdateEngineClient::GetLastAttemptError(
    int32_t* last_attempt_error) const {
  return proxy_->GetLastAttemptError(last_attempt_error, nullptr);
}

}  // namespace internal
}  // namespace update_engine
