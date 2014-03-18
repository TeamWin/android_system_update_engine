// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_SERVICE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_SERVICE_H_

#include <inttypes.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib-object.h>

#include "update_engine/update_attempter.h"

// Type macros:
#define UPDATE_ENGINE_TYPE_SERVICE (update_engine_service_get_type())
#define UPDATE_ENGINE_SERVICE(obj)                                      \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), UPDATE_ENGINE_TYPE_SERVICE,        \
                              UpdateEngineService))
#define UPDATE_ENGINE_IS_SERVICE(obj)                                   \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), UPDATE_ENGINE_TYPE_SERVICE))
#define UPDATE_ENGINE_SERVICE_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), UPDATE_ENGINE_TYPE_SERVICE, \
                           UpdateEngineService))
#define UPDATE_ENGINE_IS_SERVICE_CLASS(klass)                           \
  (G_TYPE_CHECK_CLASS_TYPE((klass), UPDATE_ENGINE_TYPE_SERVICE))
#define UPDATE_ENGINE_SERVICE_GET_CLASS(obj)                    \
  (G_TYPE_INSTANCE_GET_CLASS((obj), UPDATE_ENGINE_TYPE_SERVICE, \
                             UpdateEngineService))

G_BEGIN_DECLS

struct UpdateEngineService {
  GObject parent_instance;

  chromeos_update_engine::SystemState* system_state_;
};

struct UpdateEngineServiceClass {
  GObjectClass parent_class;
};

UpdateEngineService* update_engine_service_new(void);
GType update_engine_service_get_type(void);

// Methods

gboolean update_engine_service_attempt_update(UpdateEngineService* self,
                                              gchar* app_version,
                                              gchar* omaha_url,
                                              GError **error);

gboolean update_engine_service_attempt_update_with_flags(
    UpdateEngineService* self,
    gchar* app_version,
    gchar* omaha_url,
    gint flags_as_int,
    GError **error);

gboolean update_engine_service_attempt_rollback(UpdateEngineService* self,
                                                gboolean powerwash,
                                                GError **error);

// Checks if the system rollback is available by verifying if the secondary
// system partition is valid and bootable.
gboolean update_engine_service_can_rollback(
    UpdateEngineService* self,
    gboolean* out_can_rollback,
    GError **error);

gboolean update_engine_service_reset_status(UpdateEngineService* self,
                                            GError **error);

gboolean update_engine_service_get_status(UpdateEngineService* self,
                                          int64_t* last_checked_time,
                                          double* progress,
                                          gchar** current_operation,
                                          gchar** new_version,
                                          int64_t* new_size,
                                          GError **error);

gboolean update_engine_service_reboot_if_needed(UpdateEngineService* self,
                                                GError **error);

// Changes the current channel of the device to the target channel. If the
// target channel is a less stable channel than the current channel, then the
// channel change happens immediately (at the next update check).  If the
// target channel is a more stable channel, then if is_powerwash_allowed is set
// to true, then also the change happens immediately but with a powerwash if
// required. Otherwise, the change takes effect eventually (when the version on
// the target channel goes above the version number of what the device
// currently has).
gboolean update_engine_service_set_channel(UpdateEngineService* self,
                                           gchar* target_channel,
                                           gboolean is_powerwash_allowed,
                                           GError **error);

// If get_current_channel is set to true, populates |channel| with the name of
// the channel that the device is currently on. Otherwise, it populates it with
// the name of the channel the device is supposed to be (in case of a pending
// channel change).
gboolean update_engine_service_get_channel(UpdateEngineService* self,
                                           gboolean get_current_channel,
                                           gchar** channel,
                                           GError **error);

// Enables or disables the sharing and consuming updates over P2P feature
// according to the |enabled| argument passed.
gboolean update_engine_service_set_p2p_update_permission(
    UpdateEngineService* self,
    gboolean enabled,
    GError **error);

// Returns in |enabled| the current value for the P2P enabled setting. This
// involves both sharing and consuming updates over P2P.
gboolean update_engine_service_get_p2p_update_permission(
    UpdateEngineService* self,
    gboolean* enabled,
    GError **error);

// If there's no device policy installed, sets the update over cellular networks
// permission to the |allowed| value. Otherwise, this method returns with an
// error since this setting is overridden by the applied policy.
gboolean update_engine_service_set_update_over_cellular_permission(
    UpdateEngineService* self,
    gboolean allowed,
    GError **error);

// Returns the current value of the update over cellular network setting, either
// forced by the device policy if the device is enrolled or the current user
// preference otherwise.
gboolean update_engine_service_get_update_over_cellular_permission(
    UpdateEngineService* self,
    gboolean* allowed,
    GError **error);

// Returns the duration since the last successful update, as the
// duration on the wallclock. Returns an error if the device has not
// updated.
gboolean update_engine_service_get_duration_since_update(
    UpdateEngineService* self,
    gint64* out_usec_wallclock,
    GError **error);

gboolean update_engine_service_emit_status_update(
    UpdateEngineService* self,
    gint64 last_checked_time,
    gdouble progress,
    const gchar* current_operation,
    const gchar* new_version,
    gint64 new_size);

G_END_DECLS

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_SERVICE_H_
