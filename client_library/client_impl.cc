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

#include "update_engine/client_library/client_impl.h"

#include <dbus/bus.h>
#include <update_engine/dbus-constants.h>

#include "update_engine/update_status_utils.h"

using chromeos_update_engine::StringToUpdateStatus;
using dbus::Bus;
using org::chromium::UpdateEngineInterfaceProxy;
using std::string;

namespace update_engine {
namespace internal {

UpdateEngineClientImpl::UpdateEngineClientImpl() {
  Bus::Options options;
  options.bus_type = Bus::SYSTEM;
  scoped_refptr<Bus> bus{new Bus{options}};
  proxy_.reset(new UpdateEngineInterfaceProxy{bus});
}

bool UpdateEngineClientImpl::AttemptUpdate(const string& in_app_version,
                                           const string& in_omaha_url,
                                           bool at_user_request) {
  return proxy_->AttemptUpdateWithFlags(
      in_app_version, in_omaha_url,
      (at_user_request) ? 0 : kAttemptUpdateFlagNonInteractive, nullptr);
}

bool UpdateEngineClientImpl::GetStatus(int64_t* out_last_checked_time,
                                       double* out_progress,
                                       UpdateStatus* out_update_status,
                                       string* out_new_version,
                                       int64_t* out_new_size) {
  string status_as_string;
  const bool success =
      proxy_->GetStatus(out_last_checked_time, out_progress, &status_as_string,
                        out_new_version, out_new_size, nullptr);
  if (!success) {
    return false;
  }

  return StringToUpdateStatus(status_as_string, out_update_status);
}

bool UpdateEngineClientImpl::SetUpdateOverCellularPermission(bool allowed) {
  return proxy_->SetUpdateOverCellularPermission(allowed, nullptr);
}

bool UpdateEngineClientImpl::GetUpdateOverCellularPermission(bool* allowed) {
  return proxy_->GetUpdateOverCellularPermission(allowed, nullptr);
}

bool UpdateEngineClientImpl::SetP2PUpdatePermission(bool enabled) {
  return proxy_->SetP2PUpdatePermission(enabled, nullptr);
}

bool UpdateEngineClientImpl::GetP2PUpdatePermission(bool* enabled) {
  return proxy_->GetP2PUpdatePermission(enabled, nullptr);
}

bool UpdateEngineClientImpl::Rollback(bool powerwash) {
  return proxy_->AttemptRollback(powerwash, nullptr);
}

bool UpdateEngineClientImpl::GetRollbackPartition(string* rollback_partition) {
  return proxy_->GetRollbackPartition(rollback_partition, nullptr);
}

bool UpdateEngineClientImpl::GetPrevVersion(string* prev_version) {
  return proxy_->GetPrevVersion(prev_version, nullptr);
}

void UpdateEngineClientImpl::RebootIfNeeded() {
  bool ret = proxy_->RebootIfNeeded(nullptr);
  if (!ret) {
    // Reboot error code doesn't necessarily mean that a reboot
    // failed. For example, D-Bus may be shutdown before we receive the
    // result.
    LOG(INFO) << "RebootIfNeeded() failure ignored.";
  }
}

bool UpdateEngineClientImpl::ResetStatus() {
  return proxy_->ResetStatus(nullptr);
}

void UpdateEngineClientImpl::StatusUpdateHandlerRegistered(
    StatusUpdateHandler* handler, const std::string& interface,
    const std::string& signal_name, bool success) {
  if (!success) {
    handler->IPCError("Could not connect to" + signal_name);
    return;
  }

  int64_t last_checked_time;
  double progress;
  UpdateStatus update_status;
  string new_version;
  int64_t new_size;

  if (GetStatus(&last_checked_time, &progress, &update_status, &new_version,
                &new_size)) {
    handler->HandleStatusUpdate(last_checked_time, progress, update_status,
                                new_version, new_size);
    return;
  }

  handler->IPCError("Could not query current status");
}

void UpdateEngineClientImpl::RunStatusUpdateHandler(
    StatusUpdateHandler* h, int64_t last_checked_time, double progress,
    const std::string& current_operation, const std::string& new_version,
    int64_t new_size) {
  UpdateStatus status;
  StringToUpdateStatus(current_operation, &status);

  h->HandleStatusUpdate(last_checked_time, progress, status, new_version,
                        new_size);
}

void UpdateEngineClientImpl::RegisterStatusUpdateHandler(
    StatusUpdateHandler* handler) {
  proxy_->RegisterStatusUpdateSignalHandler(
      base::Bind(&UpdateEngineClientImpl::RunStatusUpdateHandler,
                 base::Unretained(this), base::Unretained(handler)),
      base::Bind(&UpdateEngineClientImpl::StatusUpdateHandlerRegistered,
                 base::Unretained(this), base::Unretained(handler)));
}

bool UpdateEngineClientImpl::SetTargetChannel(const string& in_target_channel,
                                              bool allow_powerwash) {
  return proxy_->SetChannel(in_target_channel, allow_powerwash, nullptr);
}

bool UpdateEngineClientImpl::GetTargetChannel(string* out_channel) {
  return proxy_->GetChannel(false,  // Get the target channel.
                            out_channel, nullptr);
}

bool UpdateEngineClientImpl::GetChannel(string* out_channel) {
  return proxy_->GetChannel(true,  // Get the current channel.
                            out_channel, nullptr);
}

}  // namespace internal
}  // namespace update_engine
