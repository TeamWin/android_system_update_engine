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
#include "update_engine/policy_manager/shill_provider.h"

using chromeos_update_engine::ClockInterface;
using chromeos_update_engine::DBusWrapperInterface;

namespace chromeos_policy_manager {

// A DBus connector for making all shill related calls.
class ShillConnector {
 public:
  // Expected type for the PropertyChanged signal handler.
  typedef void (*PropertyChangedHandler)(DBusGProxy*, const char*, GValue*,
                                         void*);

  ShillConnector(DBusWrapperInterface* dbus,
                 PropertyChangedHandler signal_handler, void* signal_data)
      : dbus_(dbus), signal_handler_(signal_handler),
        signal_data_(signal_data) {}

  ~ShillConnector();

  // Initializes the DBus connector. Returns |true| on success.
  bool Init();

  // Obtains the type of a network connection described by |service_path|,
  // storing it to |*conn_type_p|. Returns |true| on success; |false| on
  // failure, in which case no value is written.
  bool GetConnectionType(const std::string& service_path,
                         ConnectionType* conn_type_p);

  // Issues a GetProperties call to shill's manager interface, storing the
  // result to |*result_p|. Returns |true| on success.
  bool GetManagerProperties(GHashTable** result_p) {
    return GetProperties(manager_proxy_, result_p);
  }

  // Converts a shill connection type string into a symbolic value.
  static ConnectionType ParseConnectionType(const char* str);

 private:
  // Issues a GetProperties call through a given |proxy|, storing the result to
  // |*result_p|. Returns |true| on success.
  bool GetProperties(DBusGProxy* proxy, GHashTable** result_p);

  struct ConnStrToType {
    const char *str;
    ConnectionType type;
  };

  // A mapping from shill connection type strings to enum values.
  static const ConnStrToType shill_conn_str_to_type[];

  // An initialization flag.
  bool is_init_ = false;

  // The DBus interface and connection, and a shill manager proxy.
  DBusWrapperInterface* dbus_;
  DBusGConnection* connection_ = NULL;
  DBusGProxy* manager_proxy_ = NULL;

  // The shill manager signal handler credentials.
  PropertyChangedHandler signal_handler_ = NULL;
  void* signal_data_ = NULL;

  // Return a DBus proxy for a given |path| and |interface| within shill.
  DBusGProxy* GetProxy(const char* path, const char* interface);

  DISALLOW_COPY_AND_ASSIGN(ShillConnector);
};

// ShillProvider concrete implementation.
class RealShillProvider : public ShillProvider {
 public:
  RealShillProvider(DBusWrapperInterface* dbus, ClockInterface* clock)
      : dbus_(dbus), clock_(clock) {}

  virtual inline Variable<bool>* var_is_connected() override {
    return var_is_connected_.get();
  }

  virtual inline Variable<ConnectionType>* var_conn_type() override {
    return var_conn_type_.get();
  }

  virtual inline Variable<base::Time>* var_conn_last_changed() override {
    return var_conn_last_changed_.get();
  }

 protected:
  virtual bool DoInit() override;

 private:
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

  // The current default service path, if connected.
  std::string default_service_path_;

  // The last known type of the default connection.
  ConnectionType conn_type_ = ConnectionType::kUnknown;

  // Whether the last known connection type is valid.
  bool is_conn_type_valid_ = false;

  // A shill DBus connector.
  scoped_ptr<ShillConnector> connector_;

  // The DBus interface object (mockable).
  DBusWrapperInterface* const dbus_;

  // A clock abstraction (mockable).
  ClockInterface* const clock_;

  // Pointers to all variable objects.
  scoped_ptr<Variable<bool>> var_is_connected_;
  scoped_ptr<Variable<ConnectionType>> var_conn_type_;
  scoped_ptr<Variable<base::Time>> var_conn_last_changed_;

  friend class ConnTypeVariable;
  DISALLOW_COPY_AND_ASSIGN(RealShillProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SHILL_PROVIDER_H_
