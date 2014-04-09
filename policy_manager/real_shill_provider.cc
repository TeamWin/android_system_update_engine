// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_shill_provider.h"

#include <string>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "update_engine/policy_manager/generic_variables.h"
#include "update_engine/utils.h"

using std::string;

namespace {

const char* kConnInfoNotAvailErrMsg = "Connection information not available";

// Looks up a key in a hash table and returns the string inside of the returned
// GValue.
const char* GetStrProperty(GHashTable* hash_table, const char* key) {
  auto gval = reinterpret_cast<GValue*>(g_hash_table_lookup(hash_table, key));
  return (gval ? g_value_get_string(gval) : NULL);
}

};  // namespace


namespace chromeos_policy_manager {

// ShillConnector methods.

const ShillConnector::ConnStrToType ShillConnector::shill_conn_str_to_type[] = {
  {shill::kTypeEthernet, ConnectionType::kEthernet},
  {shill::kTypeWifi, ConnectionType::kWifi},
  {shill::kTypeWimax, ConnectionType::kWimax},
  {shill::kTypeBluetooth, ConnectionType::kBluetooth},
  {shill::kTypeCellular, ConnectionType::kCellular},
};

ShillConnector::~ShillConnector() {
  if (!is_init_)
    return;

  // Detach signal handler, free manager proxy.
  dbus_->ProxyDisconnectSignal(manager_proxy_, shill::kMonitorPropertyChanged,
                               G_CALLBACK(signal_handler_), signal_data_);
  dbus_->ProxyUnref(manager_proxy_);
}

bool ShillConnector::Init() {
  if (is_init_)
    return true;

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
                            G_CALLBACK(signal_handler_), signal_data_, NULL);

  return is_init_ = true;
}

bool ShillConnector::GetConnectionType(const string& service_path,
                                       ConnectionType* conn_type_p) {
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

ConnectionType ShillConnector::ParseConnType(const char* str) {
  for (unsigned i = 0; i < arraysize(shill_conn_str_to_type); i++)
    if (!strcmp(str, shill_conn_str_to_type[i].str))
      return shill_conn_str_to_type[i].type;

  return ConnectionType::kUnknown;
}

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


// A variable returning the curent connection type.
class ConnTypeVariable : public Variable<ConnectionType> {
 public:
  ConnTypeVariable(const string& name, ShillConnector* connector,
                   RealShillProvider* provider)
      : Variable<ConnectionType>(name, kVariableModePoll),
        connector_(connector), provider_(provider) {}

 protected:
  // TODO(garnold) Shift to a non-blocking version, respect the timeout.
  virtual const ConnectionType* GetValue(base::TimeDelta /* timeout */,
                                         string* errmsg) {
    if (!(provider_->is_conn_status_init_)) {
      if (errmsg)
        *errmsg = kConnInfoNotAvailErrMsg;
      return NULL;
    }

    if (!(provider_->is_connected_)) {
      if (errmsg)
        *errmsg = "No connection detected";
      return NULL;
    }

    if (!provider_->is_conn_type_valid_) {
      if (!connector_->GetConnectionType(provider_->default_service_path_,
                                         &provider_->conn_type_)) {
        if (errmsg)
          *errmsg = base::StringPrintf(
              "Could not retrieve type of default connection (%s)",
              provider_->default_service_path_.c_str());
        return NULL;
      }
      provider_->is_conn_type_valid_ = true;
    }

    return new ConnectionType(provider_->conn_type_);
  }

 private:
  // The DBus connector.
  ShillConnector* connector_;

  // The shill provider object (we need to read/update some internal members).
  RealShillProvider* provider_;

  DISALLOW_COPY_AND_ASSIGN(ConnTypeVariable);
};


// RealShillProvider methods.

bool RealShillProvider::DoInit() {
  // Initialize a DBus connection and obtain the shill manager proxy.
  connector_.reset(new ShillConnector(dbus_, HandlePropertyChangedStatic,
                                      this));
  if (!connector_->Init())
    return false;

  // Attempt to read initial connection status. Even if this fails because shill
  // is not responding (e.g. it is down) we'll be notified via "PropertyChanged"
  // signal as soon as it comes up, so this is not a critical step.
  GHashTable* hash_table = NULL;
  if (connector_->GetManagerProperties(&hash_table)) {
    GValue* value = reinterpret_cast<GValue*>(
        g_hash_table_lookup(hash_table, shill::kDefaultServiceProperty));
    ProcessDefaultService(value);
    g_hash_table_unref(hash_table);
  }

  // Initialize variables.
  var_is_connected_.reset(
      new CopyVariable<bool>("is_connected", kVariableModePoll, is_connected_,
                             &is_conn_status_init_, kConnInfoNotAvailErrMsg));
  var_conn_type_.reset(
      new ConnTypeVariable("conn_type", connector_.get(), this));
  var_conn_last_changed_.reset(
      new CopyVariable<base::Time>("conn_last_changed", kVariableModePoll,
                                   conn_last_changed_, &is_conn_status_init_,
                                   kConnInfoNotAvailErrMsg));
  return true;
}

bool RealShillProvider::ProcessDefaultService(GValue* value) {
  // Decode the string from the boxed value.
  const char* default_service_path_str = NULL;
  if (!(value && (default_service_path_str = g_value_get_string(value))))
    return false;

  // Update the connection status.
  is_connected_ = strcmp(default_service_path_str, "/");
  if (default_service_path_ != default_service_path_str)
    conn_last_changed_ = clock_->GetWallclockTime();
  default_service_path_ = default_service_path_str;
  is_conn_type_valid_ = false;

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
