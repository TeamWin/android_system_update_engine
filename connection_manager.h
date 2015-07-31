// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_CONNECTION_MANAGER_H_
#define UPDATE_ENGINE_CONNECTION_MANAGER_H_

#include <base/macros.h>

#include "update_engine/connection_manager_interface.h"
#include "update_engine/dbus_wrapper_interface.h"

namespace chromeos_update_engine {

class SystemState;

// This class implements the concrete class that talks with the connection
// manager (shill) over DBus.
class ConnectionManager : public ConnectionManagerInterface {
 public:
  // Returns the string representation corresponding to the given
  // connection type.
  static const char* StringForConnectionType(NetworkConnectionType type);

  // Returns the string representation corresponding to the given tethering
  // state.
  static const char* StringForTethering(NetworkTethering tethering);

  // Constructs a new ConnectionManager object initialized with the
  // given system state.
  explicit ConnectionManager(SystemState* system_state);
  ~ConnectionManager() override = default;

  // ConnectionManagerInterface overrides
  bool GetConnectionProperties(DBusWrapperInterface* dbus_iface,
                               NetworkConnectionType* out_type,
                               NetworkTethering* out_tethering) const override;
  bool IsUpdateAllowedOver(NetworkConnectionType type,
                           NetworkTethering tethering) const override;

 private:
  // The global context for update_engine
  SystemState* system_state_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CONNECTION_MANAGER_H_
