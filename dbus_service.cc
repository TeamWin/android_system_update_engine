// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/dbus_service.h"

#include <set>
#include <string>

#include <base/logging.h>
#include <policy/device_policy.h>

#include "update_engine/clock_interface.h"
#include "update_engine/connection_manager.h"
#include "update_engine/dbus_constants.h"
#include "update_engine/hardware_interface.h"
#include "update_engine/marshal.glibmarshal.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/p2p_manager.h"
#include "update_engine/prefs.h"
#include "update_engine/update_attempter.h"
#include "update_engine/utils.h"

using std::set;
using std::string;
using chromeos_update_engine::AttemptUpdateFlags;
using chromeos_update_engine::kAttemptUpdateFlagNonInteractive;

static const char kAUTestURLRequest[] = "autest";
// By default autest bypasses scattering. If we want to test scattering,
// we should use autest-scheduled. The Url used is same in both cases, but
// different params are passed to CheckForUpdate method.
static const char kScheduledAUTestURLRequest[] = "autest-scheduled";

static const char kAUTestURL[] =
    "https://omaha.sandbox.google.com/service/update2";

G_DEFINE_TYPE(UpdateEngineService, update_engine_service, G_TYPE_OBJECT)

static void update_engine_service_finalize(GObject* object) {
  G_OBJECT_CLASS(update_engine_service_parent_class)->finalize(object);
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
      NULL,  // Accumulator
      NULL,  // Accumulator data
      update_engine_VOID__INT64_DOUBLE_STRING_STRING_INT64,
      G_TYPE_NONE,  // Return type
      5,  // param count:
      G_TYPE_INT64,
      G_TYPE_DOUBLE,
      G_TYPE_STRING,
      G_TYPE_STRING,
      G_TYPE_INT64);
}

static void update_engine_service_init(UpdateEngineService* object) {
}

UpdateEngineService* update_engine_service_new(void) {
  return reinterpret_cast<UpdateEngineService*>(
      g_object_new(UPDATE_ENGINE_TYPE_SERVICE, NULL));
}

gboolean update_engine_service_attempt_update(UpdateEngineService* self,
                                              gchar* app_version,
                                              gchar* omaha_url,
                                              GError **error) {
  return update_engine_service_attempt_update_with_flags(self,
                                                         app_version,
                                                         omaha_url,
                                                         0, // No flags set.
                                                         error);
}

gboolean update_engine_service_attempt_update_with_flags(
    UpdateEngineService* self,
    gchar* app_version,
    gchar* omaha_url,
    gint flags_as_int,
    GError **error) {
  string update_app_version;
  string update_omaha_url;
  AttemptUpdateFlags flags = static_cast<AttemptUpdateFlags>(flags_as_int);
  bool interactive = true;

  // Only non-official (e.g., dev and test) builds can override the current
  // version and update server URL over D-Bus. However, pointing to the
  // hardcoded test update server URL is always allowed.
  if (!self->system_state_->hardware()->IsOfficialBuild()) {
    if (app_version) {
      update_app_version = app_version;
    }
    if (omaha_url) {
      update_omaha_url = omaha_url;
    }
  }
  if (omaha_url) {
    if (strcmp(omaha_url, kScheduledAUTestURLRequest) == 0) {
      update_omaha_url = kAUTestURL;
      // pretend that it's not user-initiated even though it is,
      // so as to test scattering logic, etc. which get kicked off
      // only in scheduled update checks.
      interactive = false;
    } else if (strcmp(omaha_url, kAUTestURLRequest) == 0) {
      update_omaha_url = kAUTestURL;
    }
  }
  if (flags & kAttemptUpdateFlagNonInteractive)
    interactive = false;
  LOG(INFO) << "Attempt update: app_version=\"" << update_app_version << "\" "
            << "omaha_url=\"" << update_omaha_url << "\" "
            << "flags=0x" << std::hex << flags << " "
            << "interactive=" << (interactive? "yes" : "no");
  self->system_state_->update_attempter()->CheckForUpdate(update_app_version,
                                                          update_omaha_url,
                                                          interactive);
  return TRUE;
}

gboolean update_engine_service_attempt_rollback(UpdateEngineService* self,
                                                gboolean powerwash,
                                                GError **error) {
  LOG(INFO) << "Attempting rollback to non-active partitions.";
  return self->system_state_->update_attempter()->Rollback(powerwash, NULL);
}

gboolean update_engine_service_reset_status(UpdateEngineService* self,
                                            GError **error) {
  *error = NULL;
  return self->system_state_->update_attempter()->ResetStatus();
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
  if (!(*current_operation && *new_version)) {
    *error = NULL;
    return FALSE;
  }
  return TRUE;
}

gboolean update_engine_service_reboot_if_needed(UpdateEngineService* self,
                                                GError **error) {
  if (!self->system_state_->update_attempter()->RebootIfNeeded()) {
    *error = NULL;
    return FALSE;
  }
  return TRUE;
}

gboolean update_engine_service_set_channel(UpdateEngineService* self,
                                           gchar* target_channel,
                                           gboolean is_powerwash_allowed,
                                           GError **error) {
  if (!target_channel)
    return FALSE;

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
    LOG(INFO) << "Cannot set target channel explicitly when channel "
                 "policy/settings is not delegated";
    return FALSE;
  }

  LOG(INFO) << "Setting destination channel to: " << target_channel;
  if (!self->system_state_->request_params()->SetTargetChannel(
          target_channel, is_powerwash_allowed)) {
    *error = NULL;
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
  chromeos_update_engine::P2PManager* p2p_manager =
      self->system_state_->p2p_manager();

  bool p2p_was_enabled = p2p_manager && p2p_manager->IsP2PEnabled();

  if (!prefs->SetBoolean(chromeos_update_engine::kPrefsP2PEnabled, enabled)) {
    LOG(ERROR) << "Error setting the update over cellular to "
               << (enabled ? "true" : "false");
    *error = NULL;
    return FALSE;
  }

  // If P2P is being effectively disabled (IsP2PEnabled() reports the change)
  // then we need to shutdown the service.
  if (p2p_was_enabled && !p2p_manager->IsP2PEnabled())
    p2p_manager->EnsureP2PNotRunning();

  return TRUE;
}

gboolean update_engine_service_get_p2p_update_permission(
    UpdateEngineService* self,
    gboolean* enabled,
    GError **error) {
  chromeos_update_engine::PrefsInterface* prefs = self->system_state_->prefs();

  // The default for not present setting is false.
  if (!prefs->Exists(chromeos_update_engine::kPrefsP2PEnabled)) {
    *enabled = false;
    return TRUE;
  }

  bool p2p_pref = false;
  if (!prefs->GetBoolean(chromeos_update_engine::kPrefsP2PEnabled, &p2p_pref)) {
    LOG(ERROR) << "Error getting the P2PEnabled setting.";
    *error = NULL;
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
    LOG(INFO) << "Ignoring the update over cellular setting since there's "
                 "a device policy enforcing this setting.";
    *error = NULL;
    return FALSE;
  }

  // If the policy wasn't loaded yet, then it is still OK to change the local
  // setting because the policy will be checked again during the update check.

  chromeos_update_engine::PrefsInterface* prefs = self->system_state_->prefs();

  if (!prefs->SetBoolean(
      chromeos_update_engine::kPrefsUpdateOverCellularPermission,
      allowed)) {
    LOG(ERROR) << "Error setting the update over cellular to "
               << (allowed ? "true" : "false");
    *error = NULL;
    return FALSE;
  }

  return TRUE;
}

gboolean update_engine_service_get_update_over_cellular_permission(
    UpdateEngineService* self,
    gboolean* allowed,
    GError **/*error*/) {
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
  *allowed = cm->IsUpdateAllowedOver(chromeos_update_engine::kNetCellular);

  return TRUE;
}

gboolean update_engine_service_get_duration_since_update(
    UpdateEngineService* self,
    gint64* out_usec_wallclock,
    GError **/*error*/) {

  base::Time time;
  if (!self->system_state_->update_attempter()->GetBootTimeAtUpdate(&time))
    return FALSE;

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
