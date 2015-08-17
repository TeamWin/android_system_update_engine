// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_CONNECTION_MANAGER_H_
#define UPDATE_ENGINE_CONNECTION_MANAGER_H_

#include <string>

#include <base/macros.h>

#include "update_engine/connection_manager_interface.h"
#include "update_engine/dbus_proxies.h"
#include "update_engine/shill_proxy_interface.h"

namespace chromeos_update_engine {

class SystemState;

// This class implements the concrete class that talks with the connection
// manager (shill) over DBus.
// TODO(deymo): Remove this class and use ShillProvider from the UpdateManager.
class ConnectionManager : public ConnectionManagerInterface {
 public:
  // Returns the string representation corresponding to the given
  // connection type.
  static const char* StringForConnectionType(NetworkConnectionType type);

  // Constructs a new ConnectionManager object initialized with the
  // given system state.
  ConnectionManager(ShillProxyInterface* shill_proxy,
                    SystemState* system_state);
  ~ConnectionManager() override = default;

  // ConnectionManagerInterface overrides.
  bool GetConnectionProperties(NetworkConnectionType* out_type,
                               NetworkTethering* out_tethering) override;
  bool IsUpdateAllowedOver(NetworkConnectionType type,
                           NetworkTethering tethering) const override;

 private:
  // Returns (via out_path) the default network path, or empty string if
  // there's no network up. Returns true on success.
  bool GetDefaultServicePath(std::string* out_path);

  bool GetServicePathProperties(const std::string& path,
                                NetworkConnectionType* out_type,
                                NetworkTethering* out_tethering);

  // The mockable interface to access the shill DBus proxies.
  ShillProxyInterface* shill_proxy_;

  // The global context for update_engine.
  SystemState* system_state_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CONNECTION_MANAGER_H_
