// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/connection_manager.h"

#include <set>
#include <string>

#include <base/stl_util.h>
#include <base/strings/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus-glib.h>
#include <glib.h>
#include <policy/device_policy.h>

#include "update_engine/prefs.h"
#include "update_engine/system_state.h"
#include "update_engine/utils.h"

using std::set;
using std::string;

namespace chromeos_update_engine {

namespace {

// Gets the DbusGProxy for FlimFlam. Must be free'd with ProxyUnref()
bool GetFlimFlamProxy(DBusWrapperInterface* dbus_iface,
                      const char* path,
                      const char* interface,
                      DBusGProxy** out_proxy) {
  DBusGConnection* bus;
  DBusGProxy* proxy;
  GError* error = nullptr;

  bus = dbus_iface->BusGet(DBUS_BUS_SYSTEM, &error);
  if (!bus) {
    LOG(ERROR) << "Failed to get system bus";
    return false;
  }
  proxy = dbus_iface->ProxyNewForName(bus, shill::kFlimflamServiceName, path,
                                      interface);
  *out_proxy = proxy;
  return true;
}

// On success, caller owns the GHashTable at out_hash_table.
// Returns true on success.
bool GetProperties(DBusWrapperInterface* dbus_iface,
                   const char* path,
                   const char* interface,
                   GHashTable** out_hash_table) {
  DBusGProxy* proxy;
  GError* error = nullptr;

  TEST_AND_RETURN_FALSE(GetFlimFlamProxy(dbus_iface,
                                         path,
                                         interface,
                                         &proxy));

  gboolean rc = dbus_iface->ProxyCall_0_1(proxy,
                                          "GetProperties",
                                          &error,
                                          out_hash_table);
  dbus_iface->ProxyUnref(proxy);
  if (rc == FALSE) {
    LOG(ERROR) << "dbus_g_proxy_call failed";
    return false;
  }

  return true;
}

// Returns (via out_path) the default network path, or empty string if
// there's no network up.
// Returns true on success.
bool GetDefaultServicePath(DBusWrapperInterface* dbus_iface, string* out_path) {
  GHashTable* hash_table = nullptr;

  TEST_AND_RETURN_FALSE(GetProperties(dbus_iface,
                                      shill::kFlimflamServicePath,
                                      shill::kFlimflamManagerInterface,
                                      &hash_table));

  GValue* value = reinterpret_cast<GValue*>(g_hash_table_lookup(hash_table,
                                                                "Services"));
  GPtrArray* array = nullptr;
  bool success = false;
  if (G_VALUE_HOLDS(value, DBUS_TYPE_G_OBJECT_PATH_ARRAY) &&
      (array = reinterpret_cast<GPtrArray*>(g_value_get_boxed(value))) &&
      (array->len > 0)) {
    *out_path = static_cast<const char*>(g_ptr_array_index(array, 0));
    success = true;
  }

  g_hash_table_unref(hash_table);
  return success;
}

NetworkConnectionType ParseConnectionType(const char* type_str) {
  if (!strcmp(type_str, shill::kTypeEthernet)) {
    return NetworkConnectionType::kEthernet;
  } else if (!strcmp(type_str, shill::kTypeWifi)) {
    return NetworkConnectionType::kWifi;
  } else if (!strcmp(type_str, shill::kTypeWimax)) {
    return NetworkConnectionType::kWimax;
  } else if (!strcmp(type_str, shill::kTypeBluetooth)) {
    return NetworkConnectionType::kBluetooth;
  } else if (!strcmp(type_str, shill::kTypeCellular)) {
    return NetworkConnectionType::kCellular;
  }
  return NetworkConnectionType::kUnknown;
}

NetworkTethering ParseTethering(const char* tethering_str) {
  if (!strcmp(tethering_str, shill::kTetheringNotDetectedState)) {
    return NetworkTethering::kNotDetected;
  } else if (!strcmp(tethering_str, shill::kTetheringSuspectedState)) {
    return NetworkTethering::kSuspected;
  } else if (!strcmp(tethering_str, shill::kTetheringConfirmedState)) {
    return NetworkTethering::kConfirmed;
  }
  LOG(WARNING) << "Unknown Tethering value: " << tethering_str;
  return NetworkTethering::kUnknown;
}

bool GetServicePathProperties(DBusWrapperInterface* dbus_iface,
                              const string& path,
                              NetworkConnectionType* out_type,
                              NetworkTethering* out_tethering) {
  GHashTable* hash_table = nullptr;

  TEST_AND_RETURN_FALSE(GetProperties(dbus_iface,
                                      path.c_str(),
                                      shill::kFlimflamServiceInterface,
                                      &hash_table));

  // Populate the out_tethering.
  GValue* value =
      reinterpret_cast<GValue*>(g_hash_table_lookup(hash_table,
                                                    shill::kTetheringProperty));
  const char* tethering_str = nullptr;

  if (value != nullptr)
    tethering_str = g_value_get_string(value);
  if (tethering_str != nullptr) {
    *out_tethering = ParseTethering(tethering_str);
  } else {
    // Set to Unknown if not present.
    *out_tethering = NetworkTethering::kUnknown;
  }

  // Populate the out_type property.
  value = reinterpret_cast<GValue*>(g_hash_table_lookup(hash_table,
                                                        shill::kTypeProperty));
  const char* type_str = nullptr;
  bool success = false;
  if (value != nullptr && (type_str = g_value_get_string(value)) != nullptr) {
    success = true;
    if (!strcmp(type_str, shill::kTypeVPN)) {
      value = reinterpret_cast<GValue*>(
          g_hash_table_lookup(hash_table, shill::kPhysicalTechnologyProperty));
      if (value != nullptr &&
          (type_str = g_value_get_string(value)) != nullptr) {
        *out_type = ParseConnectionType(type_str);
      } else {
        LOG(ERROR) << "No PhysicalTechnology property found for a VPN"
                   << " connection (service: " << path << "). Returning default"
                   << " NetworkConnectionType::kUnknown value.";
        *out_type = NetworkConnectionType::kUnknown;
      }
    } else {
      *out_type = ParseConnectionType(type_str);
    }
  }
  g_hash_table_unref(hash_table);
  return success;
}

}  // namespace

ConnectionManager::ConnectionManager(SystemState *system_state)
    :  system_state_(system_state) {}

bool ConnectionManager::IsUpdateAllowedOver(NetworkConnectionType type,
                                            NetworkTethering tethering) const {
  switch (type) {
    case NetworkConnectionType::kBluetooth:
      return false;

    case NetworkConnectionType::kCellular: {
      set<string> allowed_types;
      const policy::DevicePolicy* device_policy =
          system_state_->device_policy();

      // A device_policy is loaded in a lazy way right before an update check,
      // so the device_policy should be already loaded at this point. If it's
      // not, return a safe value for this setting.
      if (!device_policy) {
        LOG(INFO) << "Disabling updates over cellular networks as there's no "
                     "device policy loaded yet.";
        return false;
      }

      if (device_policy->GetAllowedConnectionTypesForUpdate(&allowed_types)) {
        // The update setting is enforced by the device policy.

        if (!ContainsKey(allowed_types, shill::kTypeCellular)) {
          LOG(INFO) << "Disabling updates over cellular connection as it's not "
                       "allowed in the device policy.";
          return false;
        }

        LOG(INFO) << "Allowing updates over cellular per device policy.";
        return true;
      } else {
        // There's no update setting in the device policy, using the local user
        // setting.
        PrefsInterface* prefs = system_state_->prefs();

        if (!prefs || !prefs->Exists(kPrefsUpdateOverCellularPermission)) {
          LOG(INFO) << "Disabling updates over cellular connection as there's "
                       "no device policy setting nor user preference present.";
          return false;
        }

        bool stored_value;
        if (!prefs->GetBoolean(kPrefsUpdateOverCellularPermission,
                               &stored_value)) {
          return false;
        }

        if (!stored_value) {
          LOG(INFO) << "Disabling updates over cellular connection per user "
                       "setting.";
          return false;
        }
        LOG(INFO) << "Allowing updates over cellular per user setting.";
        return true;
      }
    }

    default:
      if (tethering == NetworkTethering::kConfirmed) {
        // Treat this connection as if it is a cellular connection.
        LOG(INFO) << "Current connection is confirmed tethered, using Cellular "
                     "setting.";
        return IsUpdateAllowedOver(NetworkConnectionType::kCellular,
                                   NetworkTethering::kUnknown);
      }
      return true;
  }
}

const char* ConnectionManager::StringForConnectionType(
    NetworkConnectionType type) const {
  switch (type) {
    case NetworkConnectionType::kEthernet:
      return shill::kTypeEthernet;
    case NetworkConnectionType::kWifi:
      return shill::kTypeWifi;
    case NetworkConnectionType::kWimax:
      return shill::kTypeWimax;
    case NetworkConnectionType::kBluetooth:
      return shill::kTypeBluetooth;
    case NetworkConnectionType::kCellular:
      return shill::kTypeCellular;
    case NetworkConnectionType::kUnknown:
      return "Unknown";
  }
  return "Unknown";
}

const char* ConnectionManager::StringForTethering(
    NetworkTethering tethering) const {
  switch (tethering) {
    case NetworkTethering::kNotDetected:
      return shill::kTetheringNotDetectedState;
    case NetworkTethering::kSuspected:
      return shill::kTetheringSuspectedState;
    case NetworkTethering::kConfirmed:
      return shill::kTetheringConfirmedState;
    case NetworkTethering::kUnknown:
      return "Unknown";
  }
  // The program shouldn't reach this point, but the compiler isn't smart
  // enough to infer that.
  return "Unknown";
}

bool ConnectionManager::GetConnectionProperties(
    DBusWrapperInterface* dbus_iface,
    NetworkConnectionType* out_type,
    NetworkTethering* out_tethering) const {
  string default_service_path;
  TEST_AND_RETURN_FALSE(GetDefaultServicePath(dbus_iface,
                                              &default_service_path));
  TEST_AND_RETURN_FALSE(GetServicePathProperties(dbus_iface,
                                                 default_service_path,
                                                 out_type, out_tethering));
  return true;
}

}  // namespace chromeos_update_engine
