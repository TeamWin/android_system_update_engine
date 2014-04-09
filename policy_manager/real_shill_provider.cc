// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_shill_provider.h"

#include <string>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "update_engine/utils.h"

using std::string;

namespace {

// Looks up a |key| in a GLib |hash_table| and returns the unboxed string from
// the corresponding GValue, if found.
const char* GetStrProperty(GHashTable* hash_table, const char* key) {
  auto gval = reinterpret_cast<GValue*>(g_hash_table_lookup(hash_table, key));
  return (gval ? g_value_get_string(gval) : NULL);
}

};  // namespace


namespace chromeos_policy_manager {

const char* RealShillProvider::kConnStatusUnavailable =
    "Connection status unavailable";
const char* RealShillProvider::kConnTypeUnavailable =
    "Connection type unavailable";
const char* RealShillProvider::kConnTetheringUnavailable =
    "Connection tethering mode unavailable";

RealShillProvider::~RealShillProvider() {
  // Detach signal handler, free manager proxy.
  dbus_->ProxyDisconnectSignal(manager_proxy_, shill::kMonitorPropertyChanged,
                               G_CALLBACK(HandlePropertyChangedStatic),
                               this);
  dbus_->ProxyUnref(manager_proxy_);
}

ConnectionType RealShillProvider::ParseConnectionType(const char* type_str) {
  if (!strcmp(type_str, shill::kTypeEthernet))
    return ConnectionType::kEthernet;
  if (!strcmp(type_str, shill::kTypeWifi))
    return ConnectionType::kWifi;
  if (!strcmp(type_str, shill::kTypeWimax))
    return ConnectionType::kWimax;
  if (!strcmp(type_str, shill::kTypeBluetooth))
    return ConnectionType::kBluetooth;
  if (!strcmp(type_str, shill::kTypeCellular))
    return ConnectionType::kCellular;

  return ConnectionType::kUnknown;
}

ConnectionTethering RealShillProvider::ParseConnectionTethering(
    const char* tethering_str) {
  if (!strcmp(tethering_str, shill::kTetheringNotDetectedState))
    return ConnectionTethering::kNotDetected;
  if (!strcmp(tethering_str, shill::kTetheringSuspectedState))
    return ConnectionTethering::kSuspected;
  if (!strcmp(tethering_str, shill::kTetheringConfirmedState))
    return ConnectionTethering::kConfirmed;

  return ConnectionTethering::kUnknown;
}

bool RealShillProvider::DoInit() {
  // Obtain a DBus connection.
  GError* error = NULL;
  connection_ = dbus_->BusGet(DBUS_BUS_SYSTEM, &error);
  if (!connection_) {
    LOG(ERROR) << "Failed to initialize DBus connection: "
               << chromeos_update_engine::utils::GetAndFreeGError(&error);
    return false;
  }

  // Allocate a shill manager proxy.
  manager_proxy_ = GetProxy(shill::kFlimflamServicePath,
                            shill::kFlimflamManagerInterface);

  // Subscribe to the manager's PropertyChanged signal.
  dbus_->ProxyAddSignal_2(manager_proxy_, shill::kMonitorPropertyChanged,
                          G_TYPE_STRING, G_TYPE_VALUE);
  dbus_->ProxyConnectSignal(manager_proxy_, shill::kMonitorPropertyChanged,
                            G_CALLBACK(HandlePropertyChangedStatic),
                            this, NULL);

  // Attempt to read initial connection status. Even if this fails because shill
  // is not responding (e.g. it is down) we'll be notified via "PropertyChanged"
  // signal as soon as it comes up, so this is not a critical step.
  GHashTable* hash_table = NULL;
  if (GetProperties(manager_proxy_, &hash_table)) {
    GValue* value = reinterpret_cast<GValue*>(
        g_hash_table_lookup(hash_table, shill::kDefaultServiceProperty));
    ProcessDefaultService(value);
    g_hash_table_unref(hash_table);
  }

  return true;
}

DBusGProxy* RealShillProvider::GetProxy(const char* path,
                                        const char* interface) {
  return dbus_->ProxyNewForName(connection_, shill::kFlimflamServiceName,
                                path, interface);
}

bool RealShillProvider::GetProperties(DBusGProxy* proxy,
                                      GHashTable** result_p) {
  GError* error = NULL;
  if (!dbus_->ProxyCall_0_1(proxy, shill::kGetPropertiesFunction, &error,
                            result_p)) {
    LOG(ERROR) << "Calling shill via DBus proxy failed: "
               << chromeos_update_engine::utils::GetAndFreeGError(&error);
    return false;
  }
  return true;
}

bool RealShillProvider::ProcessDefaultService(GValue* value) {
  // Decode the string from the boxed value.
  const char* default_service_path_str = NULL;
  if (!(value && (default_service_path_str = g_value_get_string(value))))
    return false;

  // Anything changed?
  if (default_service_path_ == default_service_path_str)
    return true;

  // Update the connection status, invalidate all attributes.
  default_service_path_ = default_service_path_str;
  is_connected_ = (default_service_path_ != "/");
  conn_last_changed_ = clock_->GetWallclockTime();
  conn_type_is_valid_ = false;
  conn_tethering_is_valid_ = false;

  // If connected, update the connection attributes.
  if (is_connected_) {
    DBusGProxy* service_proxy = GetProxy(default_service_path_.c_str(),
                                         shill::kFlimflamServiceInterface);
    GHashTable* hash_table = NULL;
    if (GetProperties(service_proxy, &hash_table)) {
      // Get the connection type.
      const char* type_str = GetStrProperty(hash_table, shill::kTypeProperty);
      if (type_str && !strcmp(type_str, shill::kTypeVPN)) {
        type_str = GetStrProperty(hash_table,
                                  shill::kPhysicalTechnologyProperty);
      }
      if (type_str) {
        conn_type_ = ParseConnectionType(type_str);
        conn_type_is_valid_ = true;
      } else {
        LOG(ERROR) << "Could not find connection type ("
                   << default_service_path_ << ")";
      }

      // Get the connection tethering mode.
      const char* tethering_str = GetStrProperty(hash_table,
                                                 shill::kTetheringProperty);
      if (tethering_str) {
        conn_tethering_ = ParseConnectionTethering(tethering_str);
        conn_tethering_is_valid_ = true;
      } else {
        LOG(ERROR) << "Could not find connection tethering mode ("
                   << default_service_path_ << ")";
      }

      g_hash_table_unref(hash_table);
    }
    dbus_->ProxyUnref(service_proxy);
  }

  // Mark the connection status as initialized.
  is_conn_status_init_ = true;
  return true;
}

void RealShillProvider::HandlePropertyChanged(DBusGProxy* proxy,
                                              const char* name, GValue* value) {
  if (!strcmp(name, shill::kDefaultServiceProperty))
    ProcessDefaultService(value);
}

void RealShillProvider::HandlePropertyChangedStatic(DBusGProxy* proxy,
                                                    const char* name,
                                                    GValue* value,
                                                    void* data) {
  auto obj = reinterpret_cast<RealShillProvider*>(data);
  obj->HandlePropertyChanged(proxy, name, value);
}

}  // namespace chromeos_policy_manager
