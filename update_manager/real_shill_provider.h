// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_REAL_SHILL_PROVIDER_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_REAL_SHILL_PROVIDER_H_

// TODO(garnold) Much of the functionality in this module was adapted from the
// update engine's connection_manager.  We need to make sure to deprecate use of
// connection manager when the time comes.

#include <string>

#include <base/time/time.h>

#include "update_engine/clock_interface.h"
#include "update_engine/dbus_proxies.h"
#include "update_engine/shill_proxy_interface.h"
#include "update_engine/update_manager/generic_variables.h"
#include "update_engine/update_manager/shill_provider.h"

namespace chromeos_update_manager {

// ShillProvider concrete implementation.
class RealShillProvider : public ShillProvider {
 public:
  RealShillProvider(chromeos_update_engine::ShillProxyInterface* shill_proxy,
                    chromeos_update_engine::ClockInterface* clock)
      : shill_proxy_(shill_proxy), clock_(clock) {}

  ~RealShillProvider() override = default;

  // Initializes the provider and returns whether it succeeded.
  bool Init();

  Variable<bool>* var_is_connected() override {
    return &var_is_connected_;
  }

  Variable<ConnectionType>* var_conn_type() override {
    return &var_conn_type_;
  }

  Variable<ConnectionTethering>* var_conn_tethering() override {
    return &var_conn_tethering_;
  }

  Variable<base::Time>* var_conn_last_changed() override {
    return &var_conn_last_changed_;
  }

  // Helper methods for converting shill strings into symbolic values.
  static ConnectionType ParseConnectionType(const std::string& type_str);
  static ConnectionTethering ParseConnectionTethering(
      const std::string& tethering_str);

 private:
  // A handler for ManagerProxy.PropertyChanged signal.
  void OnManagerPropertyChanged(const std::string& name,
                                const chromeos::Any& value);

  // Called when the signal in ManagerProxy.PropertyChanged is connected.
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool successful);

  // Get the connection and populate the type and tethering status of the given
  // default connection.
  bool ProcessDefaultService(const std::string& default_service_path);

  // The current default service path, if connected.
  std::string default_service_path_;

  // The mockable interface to access the shill DBus proxies, owned by the
  // caller.
  chromeos_update_engine::ShillProxyInterface* shill_proxy_;

  // A clock abstraction (mockable).
  chromeos_update_engine::ClockInterface* const clock_;

  // The provider's variables.
  AsyncCopyVariable<bool> var_is_connected_{"is_connected"};
  AsyncCopyVariable<ConnectionType> var_conn_type_{"conn_type"};
  AsyncCopyVariable<ConnectionTethering> var_conn_tethering_{"conn_tethering"};
  AsyncCopyVariable<base::Time> var_conn_last_changed_{"conn_last_changed"};

  DISALLOW_COPY_AND_ASSIGN(RealShillProvider);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_REAL_SHILL_PROVIDER_H_
