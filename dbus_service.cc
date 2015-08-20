//
// Copyright (C) 2012 The Android Open Source Project
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

#include "update_engine/dbus_service.h"

#include <set>
#include <string>

#include <base/location.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/message_loops/message_loop.h>
#include <chromeos/strings/string_utils.h>
#include <policy/device_policy.h>

#include "update_engine/clock_interface.h"
#include "update_engine/connection_manager_interface.h"
#include "update_engine/dbus_constants.h"
#include "update_engine/hardware_interface.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/p2p_manager.h"
#include "update_engine/prefs.h"
#include "update_engine/update_attempter.h"
#include "update_engine/utils.h"

using base::StringPrintf;
using chromeos::ErrorPtr;
using chromeos::string_utils::ToString;
using chromeos_update_engine::AttemptUpdateFlags;
using chromeos_update_engine::kAttemptUpdateFlagNonInteractive;
using std::set;
using std::string;

namespace {
// Log and set the error on the passed ErrorPtr.
void LogAndSetError(ErrorPtr *error,
                    const tracked_objects::Location& location,
                    const string& reason) {
  chromeos::Error::AddTo(
      error, location,
      chromeos::errors::dbus::kDomain,
      chromeos_update_engine::kUpdateEngineServiceErrorFailed, reason);
  LOG(ERROR) << "Sending DBus Failure: " << location.ToString() << ": "
             << reason;
}
}  // namespace

namespace chromeos_update_engine {

UpdateEngineService::UpdateEngineService(SystemState* system_state)
    : system_state_(system_state) {}

// org::chromium::UpdateEngineInterfaceInterface methods implementation.

bool UpdateEngineService::AttemptUpdate(ErrorPtr* error,
                                        const string& in_app_version,
                                        const string& in_omaha_url) {
  return AttemptUpdateWithFlags(
      error, in_app_version, in_omaha_url, 0 /* no flags */);
}

bool UpdateEngineService::AttemptUpdateWithFlags(ErrorPtr* /* error */,
                                                 const string& in_app_version,
                                                 const string& in_omaha_url,
                                                 int32_t in_flags_as_int) {
  AttemptUpdateFlags flags = static_cast<AttemptUpdateFlags>(in_flags_as_int);
  bool interactive = !(flags & kAttemptUpdateFlagNonInteractive);

  LOG(INFO) << "Attempt update: app_version=\"" << in_app_version << "\" "
            << "omaha_url=\"" << in_omaha_url << "\" "
            << "flags=0x" << std::hex << flags << " "
            << "interactive=" << (interactive? "yes" : "no");
  system_state_->update_attempter()->CheckForUpdate(
      in_app_version, in_omaha_url, interactive);
  return true;
}

bool UpdateEngineService::AttemptRollback(ErrorPtr* error,
                                          bool in_powerwash) {
  LOG(INFO) << "Attempting rollback to non-active partitions.";

  if (!system_state_->update_attempter()->Rollback(in_powerwash)) {
    // TODO(dgarrett): Give a more specific error code/reason.
    LogAndSetError(error, FROM_HERE, "Rollback attempt failed.");
    return false;
  }
  return true;
}

bool UpdateEngineService::CanRollback(ErrorPtr* /* error */,
                                      bool* out_can_rollback) {
  bool can_rollback = system_state_->update_attempter()->CanRollback();
  LOG(INFO) << "Checking to see if we can rollback . Result: " << can_rollback;
  *out_can_rollback = can_rollback;
  return true;
}

bool UpdateEngineService::ResetStatus(ErrorPtr* error) {
  if (!system_state_->update_attempter()->ResetStatus()) {
    // TODO(dgarrett): Give a more specific error code/reason.
    LogAndSetError(error, FROM_HERE, "ResetStatus failed.");
    return false;
  }
  return true;
}

bool UpdateEngineService::GetStatus(ErrorPtr* error,
                                    int64_t* out_last_checked_time,
                                    double* out_progress,
                                    string* out_current_operation,
                                    string* out_new_version,
                                    int64_t* out_new_size) {
  if (!system_state_->update_attempter()->GetStatus(out_last_checked_time,
                                                    out_progress,
                                                    out_current_operation,
                                                    out_new_version,
                                                    out_new_size)) {
    LogAndSetError(error, FROM_HERE, "GetStatus failed.");
    return false;
  }
  return true;
}

bool UpdateEngineService::RebootIfNeeded(ErrorPtr* error) {
  if (!system_state_->update_attempter()->RebootIfNeeded()) {
    // TODO(dgarrett): Give a more specific error code/reason.
    LogAndSetError(error, FROM_HERE, "Reboot not needed, or attempt failed.");
    return false;
  }
  return true;
}

bool UpdateEngineService::SetChannel(ErrorPtr* error,
                                     const string& in_target_channel,
                                     bool in_is_powerwash_allowed) {
  const policy::DevicePolicy* device_policy = system_state_->device_policy();

  // The device_policy is loaded in a lazy way before an update check. Load it
  // now from the libchromeos cache if it wasn't already loaded.
  if (!device_policy) {
    UpdateAttempter* update_attempter = system_state_->update_attempter();
    if (update_attempter) {
      update_attempter->RefreshDevicePolicy();
      device_policy = system_state_->device_policy();
    }
  }

  bool delegated = false;
  if (device_policy &&
      device_policy->GetReleaseChannelDelegated(&delegated) && !delegated) {
    LogAndSetError(
        error, FROM_HERE,
        "Cannot set target channel explicitly when channel "
        "policy/settings is not delegated");
    return false;
  }

  LOG(INFO) << "Setting destination channel to: " << in_target_channel;
  if (!system_state_->request_params()->SetTargetChannel(
          in_target_channel, in_is_powerwash_allowed)) {
    // TODO(dgarrett): Give a more specific error code/reason.
    LogAndSetError(error, FROM_HERE, "Setting channel failed.");
    return false;
  }
  return true;
}

bool UpdateEngineService::GetChannel(ErrorPtr* /* error */,
                                     bool in_get_current_channel,
                                     string* out_channel) {
  OmahaRequestParams* rp = system_state_->request_params();
  *out_channel = (in_get_current_channel ?
                  rp->current_channel() : rp->target_channel());
  return true;
}

bool UpdateEngineService::SetP2PUpdatePermission(ErrorPtr* error,
                                                 bool in_enabled) {
  PrefsInterface* prefs = system_state_->prefs();

  if (!prefs->SetBoolean(kPrefsP2PEnabled, in_enabled)) {
    LogAndSetError(
        error, FROM_HERE,
        StringPrintf("Error setting the update via p2p permission to %s.",
                     ToString(in_enabled).c_str()));
    return false;
  }
  return true;
}

bool UpdateEngineService::GetP2PUpdatePermission(ErrorPtr* error,
                                                 bool* out_enabled) {
  PrefsInterface* prefs = system_state_->prefs();

  bool p2p_pref = false;  // Default if no setting is present.
  if (prefs->Exists(kPrefsP2PEnabled) &&
      !prefs->GetBoolean(kPrefsP2PEnabled, &p2p_pref)) {
    LogAndSetError(error, FROM_HERE, "Error getting the P2PEnabled setting.");
    return false;
  }

  *out_enabled = p2p_pref;
  return true;
}

bool UpdateEngineService::SetUpdateOverCellularPermission(ErrorPtr* error,
                                                          bool in_allowed) {
  set<string> allowed_types;
  const policy::DevicePolicy* device_policy = system_state_->device_policy();

  // The device_policy is loaded in a lazy way before an update check. Load it
  // now from the libchromeos cache if it wasn't already loaded.
  if (!device_policy) {
    UpdateAttempter* update_attempter = system_state_->update_attempter();
    if (update_attempter) {
      update_attempter->RefreshDevicePolicy();
      device_policy = system_state_->device_policy();
    }
  }

  // Check if this setting is allowed by the device policy.
  if (device_policy &&
      device_policy->GetAllowedConnectionTypesForUpdate(&allowed_types)) {
    LogAndSetError(error, FROM_HERE,
                   "Ignoring the update over cellular setting since there's "
                   "a device policy enforcing this setting.");
    return false;
  }

  // If the policy wasn't loaded yet, then it is still OK to change the local
  // setting because the policy will be checked again during the update check.

  PrefsInterface* prefs = system_state_->prefs();

  if (!prefs->SetBoolean(kPrefsUpdateOverCellularPermission, in_allowed)) {
    LogAndSetError(error, FROM_HERE,
                   string("Error setting the update over cellular to ") +
                   (in_allowed ? "true" : "false"));
    return false;
  }
  return true;
}

bool UpdateEngineService::GetUpdateOverCellularPermission(ErrorPtr* /* error */,
                                                          bool* out_allowed) {
  ConnectionManagerInterface* cm = system_state_->connection_manager();

  // The device_policy is loaded in a lazy way before an update check and is
  // used to determine if an update is allowed over cellular. Load the device
  // policy now from the libchromeos cache if it wasn't already loaded.
  if (!system_state_->device_policy()) {
    UpdateAttempter* update_attempter = system_state_->update_attempter();
    if (update_attempter)
      update_attempter->RefreshDevicePolicy();
  }

  // Return the current setting based on the same logic used while checking for
  // updates. A log message could be printed as the result of this test.
  LOG(INFO) << "Checking if updates over cellular networks are allowed:";
  *out_allowed = cm->IsUpdateAllowedOver(
      chromeos_update_engine::NetworkConnectionType::kCellular,
      chromeos_update_engine::NetworkTethering::kUnknown);
  return true;
}

bool UpdateEngineService::GetDurationSinceUpdate(ErrorPtr* error,
                                                 int64_t* out_usec_wallclock) {
  base::Time time;
  if (!system_state_->update_attempter()->GetBootTimeAtUpdate(&time)) {
    LogAndSetError(error, FROM_HERE, "No pending update.");
    return false;
  }

  ClockInterface* clock = system_state_->clock();
  *out_usec_wallclock = (clock->GetBootTime() - time).InMicroseconds();
  return true;
}

bool UpdateEngineService::GetPrevVersion(ErrorPtr* /* error */,
                                         string* out_prev_version) {
  *out_prev_version = system_state_->update_attempter()->GetPrevVersion();
  return true;
}

bool UpdateEngineService::GetKernelDevices(ErrorPtr* /* error */,
                                           string* out_kernel_devices) {
  auto devices = system_state_->update_attempter()->GetKernelDevices();
  string info;
  for (const auto& device : devices) {
    base::StringAppendF(&info, "%d:%s\n",
                        device.second ? 1 : 0, device.first.c_str());
  }
  LOG(INFO) << "Available kernel devices: " << info;
  *out_kernel_devices = info;
  return true;
}

bool UpdateEngineService::GetRollbackPartition(
    ErrorPtr* /* error */,
    string* out_rollback_partition_name) {
  string name = system_state_->update_attempter()->GetRollbackPartition();
  LOG(INFO) << "Getting rollback partition name. Result: " << name;
  *out_rollback_partition_name = name;
  return true;
}


UpdateEngineAdaptor::UpdateEngineAdaptor(SystemState* system_state,
                                         const scoped_refptr<dbus::Bus>& bus)
    : org::chromium::UpdateEngineInterfaceAdaptor(&dbus_service_),
    bus_(bus),
    dbus_service_(system_state),
    dbus_object_(nullptr, bus, dbus::ObjectPath(kUpdateEngineServicePath)) {}

void UpdateEngineAdaptor::RegisterAsync(
    const base::Callback<void(bool)>& completion_callback) {
  RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(completion_callback);
}

bool UpdateEngineAdaptor::RequestOwnership() {
  return bus_->RequestOwnershipAndBlock(kUpdateEngineServiceName,
                                        dbus::Bus::REQUIRE_PRIMARY);
}

}  // namespace chromeos_update_engine
