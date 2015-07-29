// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/dbus_service.h"

#include <set>
#include <string>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <chromeos/strings/string_utils.h>
#include <policy/device_policy.h>

#include "update_engine/clock_interface.h"
#include "update_engine/connection_manager.h"
#include "update_engine/dbus_constants.h"
#include "update_engine/hardware_interface.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/p2p_manager.h"
#include "update_engine/prefs.h"
#include "update_engine/update_attempter.h"
#include "update_engine/utils.h"

using base::StringPrintf;
using chromeos::string_utils::ToString;
using chromeos_update_engine::AttemptUpdateFlags;
using chromeos_update_engine::kAttemptUpdateFlagNonInteractive;
using std::set;
using std::string;

#define UPDATE_ENGINE_SERVICE_ERROR update_engine_service_error_quark ()
#define UPDATE_ENGINE_SERVICE_TYPE_ERROR \
  (update_engine_service_error_get_type())

enum UpdateEngineServiceError {
  UPDATE_ENGINE_SERVICE_ERROR_FAILED,
  UPDATE_ENGINE_SERVICE_NUM_ERRORS
};

static GQuark update_engine_service_error_quark(void) {
  static GQuark ret = 0;

  if (ret == 0)
    ret = g_quark_from_static_string("update_engine_service_error");

  return ret;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

static GType update_engine_service_error_get_type(void) {
  static GType etype = 0;

  if (etype == 0) {
    static const GEnumValue values[] = {
      ENUM_ENTRY(UPDATE_ENGINE_SERVICE_ERROR_FAILED, "Failed"),
      { 0, 0, 0 }
    };
    G_STATIC_ASSERT(UPDATE_ENGINE_SERVICE_NUM_ERRORS ==
                    G_N_ELEMENTS(values) - 1);
    etype = g_enum_register_static("UpdateEngineServiceError", values);
  }

  return etype;
}

G_DEFINE_TYPE(UpdateEngineService, update_engine_service, G_TYPE_OBJECT)

static void update_engine_service_finalize(GObject* object) {
  G_OBJECT_CLASS(update_engine_service_parent_class)->finalize(object);
}

static void log_and_set_response_error(GError** error,
                                       UpdateEngineServiceError error_code,
                                       const string& reason) {
  LOG(ERROR) << "Sending DBus Failure: " << reason;
  g_set_error_literal(error, UPDATE_ENGINE_SERVICE_ERROR,
                      error_code, reason.c_str());
}

static guint status_update_signal = 0;

static void update_engine_service_class_init(UpdateEngineServiceClass* klass) {
  GObjectClass *object_class;
  object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = update_engine_service_finalize;

  status_update_signal = g_signal_new(
      "status_update",
      G_OBJECT_CLASS_TYPE(klass),
      G_SIGNAL_RUN_LAST,
      0,  // 0 == no class method associated
      nullptr,  // Accumulator
      nullptr,  // Accumulator data
      nullptr,  // Marshaller
      G_TYPE_NONE,  // Return type
      5,  // param count:
      G_TYPE_INT64,
      G_TYPE_DOUBLE,
      G_TYPE_STRING,
      G_TYPE_STRING,
      G_TYPE_INT64);
}

static void update_engine_service_init(UpdateEngineService* object) {
  dbus_g_error_domain_register(UPDATE_ENGINE_SERVICE_ERROR,
                               "org.chromium.UpdateEngine.Error",
                               UPDATE_ENGINE_SERVICE_TYPE_ERROR);
}

UpdateEngineService* update_engine_service_new(void) {
  return reinterpret_cast<UpdateEngineService*>(
      g_object_new(UPDATE_ENGINE_TYPE_SERVICE, nullptr));
}

gboolean update_engine_service_attempt_update(UpdateEngineService* self,
                                              gchar* app_version,
                                              gchar* omaha_url,
                                              GError **error) {
  return update_engine_service_attempt_update_with_flags(self,
                                                         app_version,
                                                         omaha_url,
                                                         0,  // No flags set.
                                                         error);
}

gboolean update_engine_service_attempt_update_with_flags(
    UpdateEngineService* self,
    gchar* app_version,
    gchar* omaha_url,
    gint flags_as_int,
    GError **error) {
  string app_version_string, omaha_url_string;
  AttemptUpdateFlags flags = static_cast<AttemptUpdateFlags>(flags_as_int);
  bool interactive = !(flags & kAttemptUpdateFlagNonInteractive);

  if (app_version)
    app_version_string = app_version;
  if (omaha_url)
    omaha_url_string = omaha_url;

  LOG(INFO) << "Attempt update: app_version=\"" << app_version_string << "\" "
            << "omaha_url=\"" << omaha_url_string << "\" "
            << "flags=0x" << std::hex << flags << " "
            << "interactive=" << (interactive? "yes" : "no");
  self->system_state_->update_attempter()->CheckForUpdate(app_version_string,
                                                          omaha_url_string,
                                                          interactive);
  return TRUE;
}

gboolean update_engine_service_attempt_rollback(UpdateEngineService* self,
                                                gboolean powerwash,
                                                GError **error) {
  LOG(INFO) << "Attempting rollback to non-active partitions.";

  if (!self->system_state_->update_attempter()->Rollback(powerwash)) {
    // TODO(dgarrett): Give a more specific error code/reason.
    log_and_set_response_error(error,
                               UPDATE_ENGINE_SERVICE_ERROR_FAILED,
                               "Rollback attempt failed.");
    return FALSE;
  }

  return TRUE;
}

gboolean update_engine_service_can_rollback(UpdateEngineService* self,
                                            gboolean* out_can_rollback,
                                            GError **error) {
  bool can_rollback = self->system_state_->update_attempter()->CanRollback();
  LOG(INFO) << "Checking to see if we can rollback . Result: " << can_rollback;
  *out_can_rollback = can_rollback;
  return TRUE;
}

gboolean update_engine_service_get_rollback_partition(
    UpdateEngineService* self,
    gchar** out_rollback_partition_name,
    GError **error) {
  auto name = self->system_state_->update_attempter()->GetRollbackPartition();
  LOG(INFO) << "Getting rollback partition name. Result: " << name;
  *out_rollback_partition_name = g_strdup(name.c_str());
  return TRUE;
}

gboolean update_engine_service_get_kernel_devices(UpdateEngineService* self,
                                                  gchar** out_kernel_devices,
                                                  GError **error) {
  auto devices = self->system_state_->update_attempter()->GetKernelDevices();
  string info;
  for (const auto& device : devices) {
    base::StringAppendF(&info, "%d:%s\n",
                        device.second ? 1 : 0, device.first.c_str());
  }
  LOG(INFO) << "Available kernel devices: " << info;
  *out_kernel_devices = g_strdup(info.c_str());
  return TRUE;
}


gboolean update_engine_service_reset_status(UpdateEngineService* self,
                                            GError **error) {
  if (!self->system_state_->update_attempter()->ResetStatus()) {
    // TODO(dgarrett): Give a more specific error code/reason.
    log_and_set_response_error(error,
                               UPDATE_ENGINE_SERVICE_ERROR_FAILED,
                               "ResetStatus failed.");
    return FALSE;
  }

  return TRUE;
}

gboolean update_engine_service_get_status(UpdateEngineService* self,
                                          int64_t* last_checked_time,
                                          double* progress,
                                          gchar** current_operation,
                                          gchar** new_version,
                                          int64_t* new_size,
                                          GError **error) {
  string current_op;
  string new_version_str;

  CHECK(self->system_state_->update_attempter()->GetStatus(last_checked_time,
                                                           progress,
                                                           &current_op,
                                                           &new_version_str,
                                                           new_size));
  *current_operation = g_strdup(current_op.c_str());
  *new_version = g_strdup(new_version_str.c_str());

  if (!*current_operation) {
    log_and_set_response_error(error,
                               UPDATE_ENGINE_SERVICE_ERROR_FAILED,
                               "Unable to find current_operation.");
    return FALSE;
  }

  if (!*new_version) {
    log_and_set_response_error(error,
                               UPDATE_ENGINE_SERVICE_ERROR_FAILED,
                               "Unable to find vew_version.");
    return FALSE;
  }

  return TRUE;
}

gboolean update_engine_service_reboot_if_needed(UpdateEngineService* self,
                                                GError **error) {
  if (!self->system_state_->update_attempter()->RebootIfNeeded()) {
    // TODO(dgarrett): Give a more specific error code/reason.
    log_and_set_response_error(error,
                               UPDATE_ENGINE_SERVICE_ERROR_FAILED,
                               "Reboot not needed, or attempt failed.");
    return FALSE;
  }
  return TRUE;
}

gboolean update_engine_service_set_channel(UpdateEngineService* self,
                                           gchar* target_channel,
                                           gboolean is_powerwash_allowed,
                                           GError **error) {
  if (!target_channel) {
    log_and_set_response_error(error,
                               UPDATE_ENGINE_SERVICE_ERROR_FAILED,
                               "Target channel to set not specified.");
    return FALSE;
  }

  const policy::DevicePolicy* device_policy =
      self->system_state_->device_policy();

  // The device_policy is loaded in a lazy way before an update check. Load it
  // now from the libchromeos cache if it wasn't already loaded.
  if (!device_policy) {
    chromeos_update_engine::UpdateAttempter* update_attempter =
        self->system_state_->update_attempter();
    if (update_attempter) {
      update_attempter->RefreshDevicePolicy();
      device_policy = self->system_state_->device_policy();
    }
  }

  bool delegated = false;
  if (device_policy &&
      device_policy->GetReleaseChannelDelegated(&delegated) && !delegated) {
    log_and_set_response_error(
        error, UPDATE_ENGINE_SERVICE_ERROR_FAILED,
        "Cannot set target channel explicitly when channel "
        "policy/settings is not delegated");
    return FALSE;
  }

  LOG(INFO) << "Setting destination channel to: " << target_channel;
  if (!self->system_state_->request_params()->SetTargetChannel(
          target_channel, is_powerwash_allowed)) {
    // TODO(dgarrett): Give a more specific error code/reason.
    log_and_set_response_error(error,
                               UPDATE_ENGINE_SERVICE_ERROR_FAILED,
                               "Setting channel failed.");
    return FALSE;
  }

  return TRUE;
}

gboolean update_engine_service_get_channel(UpdateEngineService* self,
                                           gboolean get_current_channel,
                                           gchar** channel,
                                           GError **error) {
  chromeos_update_engine::OmahaRequestParams* rp =
      self->system_state_->request_params();

  string channel_str = get_current_channel ?
      rp->current_channel() : rp->target_channel();

  *channel = g_strdup(channel_str.c_str());
  return TRUE;
}

gboolean update_engine_service_set_p2p_update_permission(
    UpdateEngineService* self,
    gboolean enabled,
    GError **error) {
  chromeos_update_engine::PrefsInterface* prefs = self->system_state_->prefs();

  if (!prefs->SetBoolean(chromeos_update_engine::kPrefsP2PEnabled, enabled)) {
    log_and_set_response_error(
        error, UPDATE_ENGINE_SERVICE_ERROR_FAILED,
        StringPrintf("Error setting the update via p2p permission to %s.",
                     ToString(enabled).c_str()));
    return FALSE;
  }

  return TRUE;
}

gboolean update_engine_service_get_p2p_update_permission(
    UpdateEngineService* self,
    gboolean* enabled,
    GError **error) {
  chromeos_update_engine::PrefsInterface* prefs = self->system_state_->prefs();

  bool p2p_pref = false;  // Default if no setting is present.
  if (prefs->Exists(chromeos_update_engine::kPrefsP2PEnabled) &&
      !prefs->GetBoolean(chromeos_update_engine::kPrefsP2PEnabled, &p2p_pref)) {
    log_and_set_response_error(error, UPDATE_ENGINE_SERVICE_ERROR_FAILED,
                               "Error getting the P2PEnabled setting.");
    return FALSE;
  }

  *enabled = p2p_pref;
  return TRUE;
}

gboolean update_engine_service_set_update_over_cellular_permission(
    UpdateEngineService* self,
    gboolean allowed,
    GError **error) {
  set<string> allowed_types;
  const policy::DevicePolicy* device_policy =
      self->system_state_->device_policy();

  // The device_policy is loaded in a lazy way before an update check. Load it
  // now from the libchromeos cache if it wasn't already loaded.
  if (!device_policy) {
    chromeos_update_engine::UpdateAttempter* update_attempter =
        self->system_state_->update_attempter();
    if (update_attempter) {
      update_attempter->RefreshDevicePolicy();
      device_policy = self->system_state_->device_policy();
    }
  }

  // Check if this setting is allowed by the device policy.
  if (device_policy &&
      device_policy->GetAllowedConnectionTypesForUpdate(&allowed_types)) {
    log_and_set_response_error(
        error, UPDATE_ENGINE_SERVICE_ERROR_FAILED,
        "Ignoring the update over cellular setting since there's "
        "a device policy enforcing this setting.");
    return FALSE;
  }

  // If the policy wasn't loaded yet, then it is still OK to change the local
  // setting because the policy will be checked again during the update check.

  chromeos_update_engine::PrefsInterface* prefs = self->system_state_->prefs();

  if (!prefs->SetBoolean(
      chromeos_update_engine::kPrefsUpdateOverCellularPermission,
      allowed)) {
    log_and_set_response_error(
        error, UPDATE_ENGINE_SERVICE_ERROR_FAILED,
        string("Error setting the update over cellular to ") +
        (allowed ? "true" : "false"));
    return FALSE;
  }

  return TRUE;
}

gboolean update_engine_service_get_update_over_cellular_permission(
    UpdateEngineService* self,
    gboolean* allowed,
    GError **error) {
  chromeos_update_engine::ConnectionManager* cm =
      self->system_state_->connection_manager();

  // The device_policy is loaded in a lazy way before an update check and is
  // used to determine if an update is allowed over cellular. Load the device
  // policy now from the libchromeos cache if it wasn't already loaded.
  if (!self->system_state_->device_policy()) {
    chromeos_update_engine::UpdateAttempter* update_attempter =
        self->system_state_->update_attempter();
    if (update_attempter)
      update_attempter->RefreshDevicePolicy();
  }

  // Return the current setting based on the same logic used while checking for
  // updates. A log message could be printed as the result of this test.
  LOG(INFO) << "Checking if updates over cellular networks are allowed:";
  *allowed = cm->IsUpdateAllowedOver(
      chromeos_update_engine::NetworkConnectionType::kCellular,
      chromeos_update_engine::NetworkTethering::kUnknown);

  return TRUE;
}

gboolean update_engine_service_get_duration_since_update(
    UpdateEngineService* self,
    gint64* out_usec_wallclock,
    GError **error) {

  base::Time time;
  if (!self->system_state_->update_attempter()->GetBootTimeAtUpdate(&time)) {
    log_and_set_response_error(error, UPDATE_ENGINE_SERVICE_ERROR_FAILED,
                               "No pending update.");
    return FALSE;
  }

  chromeos_update_engine::ClockInterface *clock = self->system_state_->clock();
  *out_usec_wallclock = (clock->GetBootTime() - time).InMicroseconds();
  return TRUE;
}

gboolean update_engine_service_emit_status_update(
    UpdateEngineService* self,
    gint64 last_checked_time,
    gdouble progress,
    const gchar* current_operation,
    const gchar* new_version,
    gint64 new_size) {
  g_signal_emit(self,
                status_update_signal,
                0,
                last_checked_time,
                progress,
                current_operation,
                new_version,
                new_size);
  return TRUE;
}

gboolean update_engine_service_get_prev_version(
    UpdateEngineService* self,
    gchar** prev_version,
    GError **error) {
  string ver = self->system_state_->update_attempter()->GetPrevVersion();
  *prev_version = g_strdup(ver.c_str());
  return TRUE;
}
