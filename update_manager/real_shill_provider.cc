//
// Copyright (C) 2014 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/update_manager/real_shill_provider.h"

#include <string>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <shill/dbus-constants.h>
#include <shill/dbus-proxies.h>

using org::chromium::flimflam::ManagerProxyInterface;
using org::chromium::flimflam::ServiceProxyInterface;
using std::string;

namespace chromeos_update_manager {

ConnectionType RealShillProvider::ParseConnectionType(const string& type_str) {
  if (type_str == shill::kTypeEthernet) {
    return ConnectionType::kEthernet;
  } else if (type_str == shill::kTypeWifi) {
    return ConnectionType::kWifi;
  } else if (type_str == shill::kTypeWimax) {
    return ConnectionType::kWimax;
  } else if (type_str == shill::kTypeBluetooth) {
    return ConnectionType::kBluetooth;
  } else if (type_str == shill::kTypeCellular) {
    return ConnectionType::kCellular;
  }
  return ConnectionType::kUnknown;
}

ConnectionTethering RealShillProvider::ParseConnectionTethering(
    const string& tethering_str) {
  if (tethering_str == shill::kTetheringNotDetectedState) {
    return ConnectionTethering::kNotDetected;
  } else if (tethering_str == shill::kTetheringSuspectedState) {
    return ConnectionTethering::kSuspected;
  } else if (tethering_str == shill::kTetheringConfirmedState) {
    return ConnectionTethering::kConfirmed;
  }
  return ConnectionTethering::kUnknown;
}

bool RealShillProvider::Init() {
  ManagerProxyInterface* manager_proxy = shill_proxy_->GetManagerProxy();
  if (!manager_proxy)
    return false;

  // Subscribe to the manager's PropertyChanged signal.
  manager_proxy->RegisterPropertyChangedSignalHandler(
      base::Bind(&RealShillProvider::OnManagerPropertyChanged,
                 base::Unretained(this)),
      base::Bind(&RealShillProvider::OnSignalConnected,
                 base::Unretained(this)));

  // Attempt to read initial connection status. Even if this fails because shill
  // is not responding (e.g. it is down) we'll be notified via "PropertyChanged"
  // signal as soon as it comes up, so this is not a critical step.
  chromeos::VariantDictionary properties;
  chromeos::ErrorPtr error;
  if (!manager_proxy->GetProperties(&properties, &error))
    return true;

  const auto& prop_default_service =
      properties.find(shill::kDefaultServiceProperty);
  if (prop_default_service != properties.end()) {
    OnManagerPropertyChanged(prop_default_service->first,
                             prop_default_service->second);
  }

  return true;
}

void RealShillProvider::OnManagerPropertyChanged(const string& name,
                                                 const chromeos::Any& value) {
  if (name == shill::kDefaultServiceProperty)
    ProcessDefaultService(value.TryGet<dbus::ObjectPath>().value());
}

void RealShillProvider::OnSignalConnected(const string& interface_name,
                                          const string& signal_name,
                                          bool successful) {
  if (!successful) {
    LOG(ERROR) << "Couldn't connect to the signal " << interface_name << "."
               << signal_name;
  }
}

bool RealShillProvider::ProcessDefaultService(
    const string& default_service_path) {
  // We assume that if the service path didn't change, then the connection
  // type and the tethering status of it also didn't change.
  if (default_service_path_ == default_service_path)
    return true;

  // Update the connection status.
  default_service_path_ = default_service_path;
  bool is_connected = (default_service_path_ != "/");
  var_is_connected_.SetValue(is_connected);
  var_conn_last_changed_.SetValue(clock_->GetWallclockTime());

  if (!is_connected) {
    var_conn_type_.UnsetValue();
    var_conn_tethering_.UnsetValue();
    return true;
  }

  // We create and dispose the ServiceProxyInterface on every request.
  std::unique_ptr<ServiceProxyInterface> service =
      shill_proxy_->GetServiceForPath(default_service_path_);

  // Get the connection properties synchronously.
  chromeos::VariantDictionary properties;
  chromeos::ErrorPtr error;
  if (!service->GetProperties(&properties, &error)) {
    var_conn_type_.UnsetValue();
    var_conn_tethering_.UnsetValue();
    return false;
  }

  // Get the connection tethering mode.
  const auto& prop_tethering = properties.find(shill::kTetheringProperty);
  if (prop_tethering == properties.end()) {
    // Remove the value if not present on the service. This most likely means an
    // error in shill and the policy will handle it, but we will print a log
    // message as well for accessing an unused variable.
    var_conn_tethering_.UnsetValue();
    LOG(ERROR) << "Could not find connection type (" << default_service_path_
               << ")";
  } else {
    // If the property doesn't contain a string value, the empty string will
    // become kUnknown.
    var_conn_tethering_.SetValue(
        ParseConnectionTethering(prop_tethering->second.TryGet<string>()));
  }

  // Get the connection type.
  const auto& prop_type = properties.find(shill::kTypeProperty);
  if (prop_type == properties.end()) {
    var_conn_type_.UnsetValue();
    LOG(ERROR) << "Could not find connection tethering mode ("
               << default_service_path_ << ")";
  } else {
    string type_str = prop_type->second.TryGet<string>();
    if (type_str == shill::kTypeVPN) {
      const auto& prop_physical =
          properties.find(shill::kPhysicalTechnologyProperty);
      if (prop_physical == properties.end()) {
        LOG(ERROR) << "No PhysicalTechnology property found for a VPN"
                   << " connection (service: " << default_service_path
                   << "). Using default kUnknown value.";
        var_conn_type_.SetValue(ConnectionType::kUnknown);
      } else {
        var_conn_type_.SetValue(
            ParseConnectionType(prop_physical->second.TryGet<string>()));
      }
    } else {
      var_conn_type_.SetValue(ParseConnectionType(type_str));
    }
  }

  return true;
}

}  // namespace chromeos_update_manager
