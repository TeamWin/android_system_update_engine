// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SHILL_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SHILL_PROVIDER_H_

// TODO(garnold) Much of the functionality in this module was adapted from the
// update engine's connection_manager.  We need to make sure to deprecate use of
// connection manager when the time comes.

#include <string>

#include <base/time.h>

#include "update_engine/clock_interface.h"
#include "update_engine/dbus_wrapper_interface.h"
#include "update_engine/policy_manager/shill_provider.h"

using chromeos_update_engine::ClockInterface;
using chromeos_update_engine::DBusWrapperInterface;

namespace chromeos_policy_manager {

// A tracker for the last reported value. Whenever Update() is called with a new
// value, the current time is written to a pointed time object.
template<typename T>
class LastValueTracker {
 public:
  LastValueTracker(ClockInterface* clock, T init_val, base::Time* time_p)
      : clock_(clock), last_val_(init_val), time_p_(time_p) {}

  const T& Update(const T& curr_val) {
    if (curr_val != last_val_) {
      last_val_ = curr_val;
      *time_p_ = clock_->GetWallclockTime();
    }
    return curr_val;
  }

 private:
  ClockInterface* const clock_;
  T last_val_;
  base::Time* time_p_;
};

// A DBus connector for making shill queries.
class ShillConnector {
 public:
  ShillConnector(DBusWrapperInterface* dbus) : dbus_(dbus) {}

  ~ShillConnector() {
    if (manager_proxy_)
      dbus_->ProxyUnref(manager_proxy_);
  }

  // Initializes the DBus connector. Returns |true| on success.
  bool Init();

  // Obtains the default network connection, storing the connection status to
  // |*is_connected_p| and (if connected) the service path for the default
  // connection in |*default_service_path_p|. Returns |true| on success; |false|
  // on failure, in which case no values are written.
  bool GetDefaultConnection(bool* is_connected_p,
                            std::string* default_service_path_p);

  // Obtains the type of a network connection described by |service_path|,
  // storing it to |*conn_type_p|. Returns |true| on success; |false| on
  // failure, in which case no value is written.
  bool GetConnectionType(const std::string& service_path,
                         ShillConnType* conn_type_p);

 private:
  typedef struct {
    const char *str;
    ShillConnType type;
  } ConnStrToType;

  // A mapping from shill connection type strings to enum values.
  static const ConnStrToType shill_conn_str_to_type[];

  // The DBus interface and connection, and a shill manager proxy.
  DBusWrapperInterface* dbus_;
  DBusGConnection* connection_ = NULL;
  DBusGProxy* manager_proxy_ = NULL;

  // Return a DBus proxy for a given |path| and |interface| within shill.
  DBusGProxy* GetProxy(const char* path, const char* interface);

  // Converts a shill connection type string into a symbolic value.
  ShillConnType ParseConnType(const char* str);

  // Issues a GetProperties call through a given |proxy|, storing the result to
  // |*result_p|. Returns |true| on success.
  bool GetProperties(DBusGProxy* proxy, GHashTable** result_p);

  DISALLOW_COPY_AND_ASSIGN(ShillConnector);
};

// ShillProvider concrete implementation.
class RealShillProvider : public ShillProvider {
 public:
  RealShillProvider(DBusWrapperInterface* dbus, ClockInterface* clock)
      : conn_last_changed_(clock->GetWallclockTime()),
        dbus_(dbus), clock_(clock),
        is_connected_tracker_(clock, false, &conn_last_changed_) {}

 protected:
  virtual bool DoInit();

 private:
  // The time when the connection type last changed.
  base::Time conn_last_changed_;

  // A shill DBus connector.
  scoped_ptr<ShillConnector> connector_;

  // The DBus interface object (mockable).
  DBusWrapperInterface* const dbus_;

  // A clock abstraction (mockable).
  ClockInterface* const clock_;

  // Tracker for the latest connection status.
  LastValueTracker<bool> is_connected_tracker_;

  DISALLOW_COPY_AND_ASSIGN(RealShillProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SHILL_PROVIDER_H_
