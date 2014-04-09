// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SHILL_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SHILL_PROVIDER_H_

// TODO(garnold) Much of the functionality in this module was adapted from the
// update engine's connection_manager.  We need to make sure to deprecate use of
// connection manager when the time comes.

#include <string>

#include <base/time/time.h>

#include "update_engine/clock_interface.h"
#include "update_engine/dbus_wrapper_interface.h"
#include "update_engine/policy_manager/generic_variables.h"
#include "update_engine/policy_manager/shill_provider.h"

using chromeos_update_engine::ClockInterface;
using chromeos_update_engine::DBusWrapperInterface;

namespace chromeos_policy_manager {

// ShillProvider concrete implementation.
class RealShillProvider : public ShillProvider {
 public:
  RealShillProvider(DBusWrapperInterface* dbus, ClockInterface* clock)
      : dbus_(dbus), clock_(clock) {}

  virtual ~RealShillProvider();

  virtual inline Variable<bool>* var_is_connected() override {
    return &var_is_connected_;
  }

  virtual inline Variable<ConnectionType>* var_conn_type() override {
    return &var_conn_type_;
  }

  virtual inline Variable<ConnectionTethering>* var_conn_tethering() override {
    return &var_conn_tethering_;
  }

  virtual inline Variable<base::Time>* var_conn_last_changed() override {
    return &var_conn_last_changed_;
  }

  // Helper methods for converting shill strings into symbolic values.
  static ConnectionType ParseConnectionType(const char* type_str);
  static ConnectionTethering ParseConnectionTethering(
      const char* tethering_str);

 protected:
  virtual bool DoInit() override;

 private:
  // Default error strings for variables.
  static const char* kConnStatusUnavailable;
  static const char* kConnTypeUnavailable;
  static const char* kConnTetheringUnavailable;

  // Return a DBus proxy for a given |path| and |interface| within shill.
  DBusGProxy* GetProxy(const char* path, const char* interface);

  // Issues a GetProperties call through a given |proxy|, storing the result to
  // |*result_p|. Returns true on success.
  bool GetProperties(DBusGProxy* proxy, GHashTable** result_p);

  // Process a default connection value, update last change time as needed.
  bool ProcessDefaultService(GValue* value);

  // A handler for manager PropertyChanged signal, and a static version.
  void HandlePropertyChanged(DBusGProxy* proxy, const char *name,
                             GValue* value);
  static void HandlePropertyChangedStatic(DBusGProxy* proxy, const char* name,
                                          GValue* value, void* data);

  // Whether the connection status has been properly initialized.
  bool is_conn_status_init_ = false;

  // The time when the connection type last changed.
  base::Time conn_last_changed_;

  // The current connection status.
  bool is_connected_;

  // The default connection type and whether its value is valid.
  ConnectionType conn_type_;
  bool conn_type_is_valid_ = false;

  // The default connection tethering mode and whether its value is valid.
  ConnectionTethering conn_tethering_;
  bool conn_tethering_is_valid_ = false;

  // The current default service path, if connected.
  std::string default_service_path_;

  // The DBus interface (mockable), connection, and a shill manager proxy.
  DBusWrapperInterface* const dbus_;
  DBusGConnection* connection_ = NULL;
  DBusGProxy* manager_proxy_ = NULL;

  // A clock abstraction (mockable).
  ClockInterface* const clock_;

  // The provider's variable.
  CopyVariable<bool> var_is_connected_{
      "is_connected", kVariableModePoll, is_connected_, &is_conn_status_init_,
      kConnStatusUnavailable};
  CopyVariable<ConnectionType> var_conn_type_{
      "conn_type", kVariableModePoll, conn_type_, &conn_type_is_valid_,
      kConnTypeUnavailable};
  CopyVariable<ConnectionTethering> var_conn_tethering_{
      "conn_tethering", kVariableModePoll, conn_tethering_,
      &conn_tethering_is_valid_, kConnTetheringUnavailable};
  CopyVariable<base::Time> var_conn_last_changed_{
      "conn_last_changed", kVariableModePoll, conn_last_changed_,
      &is_conn_status_init_, kConnStatusUnavailable};

  DISALLOW_COPY_AND_ASSIGN(RealShillProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SHILL_PROVIDER_H_
