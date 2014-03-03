// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_shill_provider.h"

#include <string>

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "update_engine/policy_manager/generic_variables.h"
#include "update_engine/utils.h"

using std::string;

namespace {

// Looks up a key in a hash table and returns the string inside of the returned
// GValue.
const char* GetStrProperty(GHashTable* hash_table, const char* key) {
  auto gval = reinterpret_cast<GValue*>(g_hash_table_lookup(hash_table, key));
  return (gval ? g_value_get_string(gval) : NULL);
}

};  // namespace


namespace chromeos_policy_manager {

const ShillConnector::ConnStrToType ShillConnector::shill_conn_str_to_type[] = {
  {shill::kTypeEthernet, kShillConnTypeEthernet},
  {shill::kTypeWifi, kShillConnTypeWifi},
  {shill::kTypeWimax, kShillConnTypeWimax},
  {shill::kTypeBluetooth, kShillConnTypeBluetooth},
  {shill::kTypeCellular, kShillConnTypeCellular},
};

bool ShillConnector::Init() {
  GError* error = NULL;
  connection_ = dbus_->BusGet(DBUS_BUS_SYSTEM, &error);
  if (!connection_) {
    LOG(ERROR) << "Failed to initialize DBus connection: "
               << chromeos_update_engine::utils::GetAndFreeGError(&error);
    return false;
  }
  manager_proxy_ = GetProxy(shill::kFlimflamServicePath,
                            shill::kFlimflamManagerInterface);
  return true;
}

bool ShillConnector::GetDefaultConnection(bool* is_connected_p,
                                          string* default_service_path_p) {
  GHashTable* hash_table = NULL;
  if (!GetProperties(manager_proxy_, &hash_table))
    return false;
  GValue* value = reinterpret_cast<GValue*>(
      g_hash_table_lookup(hash_table, shill::kDefaultServiceProperty));
  const char* default_service_path_str = NULL;
  bool success = false;
  if (value && (default_service_path_str = g_value_get_string(value))) {
    success = true;
    *is_connected_p = strcmp(default_service_path_str, "/");
    if (*is_connected_p)
      *default_service_path_p = default_service_path_str;
  }
  g_hash_table_unref(hash_table);

  return success;
}

bool ShillConnector::GetConnectionType(const string& service_path,
                                       ShillConnType* conn_type_p) {
  // Obtain a proxy for the service path.
  DBusGProxy* service_proxy = GetProxy(service_path.c_str(),
                                       shill::kFlimflamServiceInterface);

  GHashTable* hash_table = NULL;
  bool success = false;
  bool is_vpn = false;
  if (GetProperties(service_proxy, &hash_table)) {
    const char* type_str = GetStrProperty(hash_table, shill::kTypeProperty);
    if (type_str && !strcmp(type_str, shill::kTypeVPN)) {
      is_vpn = true;
      type_str = GetStrProperty(hash_table, shill::kPhysicalTechnologyProperty);
    }
    if (type_str) {
      success = true;
      *conn_type_p = ParseConnType(type_str);
    }
    g_hash_table_unref(hash_table);
  }

  if (!success) {
    LOG(ERROR) << "Could not find type of "
               << (is_vpn ? "physical connection underlying VPN " : "")
               << "connection (" << service_path << ")";
  }

  dbus_->ProxyUnref(service_proxy);
  return success;
}

DBusGProxy* ShillConnector::GetProxy(const char* path, const char* interface) {
  return dbus_->ProxyNewForName(connection_, shill::kFlimflamServiceName,
                                path, interface);
}

ShillConnType ShillConnector::ParseConnType(const char* str) {
  for (unsigned i = 0; i < arraysize(shill_conn_str_to_type); i++)
    if (!strcmp(str, shill_conn_str_to_type[i].str))
      return shill_conn_str_to_type[i].type;

  return kShillConnTypeUnknown;
}

// Issues a GetProperties call through a given |proxy|, storing the result to
// |*result_p|. Returns |true| on success.
bool ShillConnector::GetProperties(DBusGProxy* proxy, GHashTable** result_p) {
  GError* error = NULL;
  if (!dbus_->ProxyCall_0_1(proxy, shill::kGetPropertiesFunction, &error,
                            result_p)) {
    LOG(ERROR) << "Calling shill via DBus proxy failed: "
               << chromeos_update_engine::utils::GetAndFreeGError(&error);
    return false;
  }
  return true;
}

// A variable returning whether or not we have a network connection.
class IsConnectedVariable : public Variable<bool> {
 public:
  IsConnectedVariable(const string& name, ShillConnector* connector,
                      LastValueTracker<bool>* is_connected_tracker)
      : Variable<bool>(name, kVariableModePoll),
        connector_(connector),
        is_connected_tracker_(is_connected_tracker) {}

 protected:
  virtual const bool* GetValue(base::TimeDelta timeout, string* errmsg) {
    bool is_connected;
    string default_service_path;
    if (!connector_->GetDefaultConnection(&is_connected, &default_service_path))
      return NULL;;

    return new bool(is_connected_tracker_->Update(is_connected));
  }

 private:
  ShillConnector* connector_;
  LastValueTracker<bool>* is_connected_tracker_;

  DISALLOW_COPY_AND_ASSIGN(IsConnectedVariable);
};

// A variable returning the curent connection type.
class ConnTypeVariable : public Variable<ShillConnType> {
 public:
  ConnTypeVariable(const string& name, ShillConnector* connector,
                   LastValueTracker<bool>* is_connected_tracker)
      : Variable<ShillConnType>(name, kVariableModePoll),
        connector_(connector),
        is_connected_tracker_(is_connected_tracker) {}

 protected:
  virtual const ShillConnType* GetValue(base::TimeDelta timeout,
                                        string* errmsg) {
    bool is_connected;
    string default_service_path;
    ShillConnType conn_type;
    if (!(connector_->GetDefaultConnection(&is_connected,
                                           &default_service_path) &&
          is_connected &&
          connector_->GetConnectionType(default_service_path, &conn_type)))
      return NULL;

    is_connected_tracker_->Update(is_connected);
    return new ShillConnType(conn_type);
  }

 private:
  ShillConnector* connector_;
  LastValueTracker<bool>* is_connected_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ConnTypeVariable);
};

// A real implementation of the ShillProvider.
bool RealShillProvider::DoInit() {
  // Initialize a DBus connection and obtain the shill manager proxy.
  connector_.reset(new ShillConnector(dbus_));
  if (!connector_->Init())
    return false;

  // Initialize variables.
  set_var_is_connected(
      new IsConnectedVariable("is_connected", connector_.get(),
                              &is_connected_tracker_));
  set_var_conn_type(
      new ConnTypeVariable("conn_type", connector_.get(),
                           &is_connected_tracker_));
  set_var_conn_last_changed(
      new CopyVariable<base::Time>("conn_last_changed", kVariableModePoll,
                                   conn_last_changed_));
  return true;
}

}  // namespace chromeos_policy_manager
