// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_DBUS_SERVICE_H_
#define UPDATE_ENGINE_DBUS_SERVICE_H_

#include <inttypes.h>

#include <string>

#include <base/memory/ref_counted.h>
#include <chromeos/errors/error.h>

#include "update_engine/update_attempter.h"

#include "update_engine/dbus_adaptor/org.chromium.UpdateEngineInterface.h"

namespace chromeos {
namespace dbus {
class Bus;
}  // namespace dbus
}  // namespace chromeos

namespace chromeos_update_engine {

class UpdateEngineService
    : public org::chromium::UpdateEngineInterfaceInterface {
 public:
  explicit UpdateEngineService(SystemState* system_state);
  virtual ~UpdateEngineService() = default;

  // Implementation of org::chromium::UpdateEngineInterfaceInterface.
  bool AttemptUpdate(chromeos::ErrorPtr* error,
                     const std::string& in_app_version,
                     const std::string& in_omaha_url) override;

  bool AttemptUpdateWithFlags(chromeos::ErrorPtr* error,
                              const std::string& in_app_version,
                              const std::string& in_omaha_url,
                              int32_t in_flags_as_int) override;

  bool AttemptRollback(chromeos::ErrorPtr* error, bool in_powerwash) override;

  // Checks if the system rollback is available by verifying if the secondary
  // system partition is valid and bootable.
  bool CanRollback(chromeos::ErrorPtr* error, bool* out_can_rollback) override;

  // Resets the status of the update_engine to idle, ignoring any applied
  // update. This is used for development only.
  bool ResetStatus(chromeos::ErrorPtr* error) override;

  // Returns the current status of the Update Engine. If an update is in
  // progress, the number of operations, size to download and overall progress
  // is reported.
  bool GetStatus(chromeos::ErrorPtr* error,
                 int64_t* out_last_checked_time,
                 double* out_progress,
                 std::string* out_current_operation,
                 std::string* out_new_version,
                 int64_t* out_new_size) override;

  // Reboots the device if an update is applied and a reboot is required.
  bool RebootIfNeeded(chromeos::ErrorPtr* error) override;

  // Changes the current channel of the device to the target channel. If the
  // target channel is a less stable channel than the current channel, then the
  // channel change happens immediately (at the next update check).  If the
  // target channel is a more stable channel, then if is_powerwash_allowed is
  // set to true, then also the change happens immediately but with a powerwash
  // if required. Otherwise, the change takes effect eventually (when the
  // version on the target channel goes above the version number of what the
  // device currently has).
  bool SetChannel(chromeos::ErrorPtr* error,
                  const std::string& in_target_channel,
                  bool in_is_powerwash_allowed) override;

  // If get_current_channel is set to true, populates |channel| with the name of
  // the channel that the device is currently on. Otherwise, it populates it
  // with the name of the channel the device is supposed to be (in case of a
  // pending channel change).
  bool GetChannel(chromeos::ErrorPtr* error,
                  bool in_get_current_channel,
                  std::string* out_channel) override;

  // Enables or disables the sharing and consuming updates over P2P feature
  // according to the |enabled| argument passed.
  bool SetP2PUpdatePermission(chromeos::ErrorPtr* error,
                              bool in_enabled) override;

  // Returns the current value for the P2P enabled setting. This involves both
  // sharing and consuming updates over P2P.
  bool GetP2PUpdatePermission(chromeos::ErrorPtr* error,
                              bool* out_enabled) override;

  // If there's no device policy installed, sets the update over cellular
  // networks permission to the |allowed| value. Otherwise, this method returns
  // with an error since this setting is overridden by the applied policy.
  bool SetUpdateOverCellularPermission(chromeos::ErrorPtr* error,
                                       bool in_allowed) override;

  // Returns the current value of the update over cellular network setting,
  // either forced by the device policy if the device is enrolled or the current
  // user preference otherwise.
  bool GetUpdateOverCellularPermission(chromeos::ErrorPtr* error,
                                       bool* out_allowed) override;

  // Returns the duration since the last successful update, as the
  // duration on the wallclock. Returns an error if the device has not
  // updated.
  bool GetDurationSinceUpdate(chromeos::ErrorPtr* error,
                              int64_t* out_usec_wallclock) override;

  // Returns the version string of OS that was used before the last reboot
  // into an updated version. This is available only when rebooting into an
  // update from previous version, otherwise an empty string is returned.
  bool GetPrevVersion(chromeos::ErrorPtr* error,
                      std::string* out_prev_version) override;

  // Returns a list of available kernel partitions and whether each of them
  // can be booted from or not.
  bool GetKernelDevices(chromeos::ErrorPtr* error,
                        std::string* out_kernel_devices) override;

  // Returns the name of kernel partition that can be rolled back into.
  bool GetRollbackPartition(chromeos::ErrorPtr* error,
                            std::string* out_rollback_partition_name) override;

 private:
  SystemState* system_state_;
};

// The UpdateEngineAdaptor class runs the UpdateEngineInterface in the fixed
// object path, without an ObjectManager notifying the interfaces, since it is
// all static and clients don't expect it to be implemented.
class UpdateEngineAdaptor : public org::chromium::UpdateEngineInterfaceAdaptor {
 public:
  UpdateEngineAdaptor(SystemState* system_state,
                      const scoped_refptr<dbus::Bus>& bus);
  ~UpdateEngineAdaptor() = default;

  // Register the DBus object with the update engine service asynchronously.
  // Calls |copmletion_callback| when done passing a boolean indicating if the
  // registration succeeded.
  void RegisterAsync(const base::Callback<void(bool)>& completion_callback);

  // Takes ownership of the well-known DBus name and returns whether it
  // succeeded.
  bool RequestOwnership();

 private:
  scoped_refptr<dbus::Bus> bus_;
  UpdateEngineService dbus_service_;
  chromeos::dbus_utils::DBusObject dbus_object_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DBUS_SERVICE_H_
