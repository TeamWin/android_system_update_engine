// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_CONNECTION_MANAGER_H_
#define UPDATE_ENGINE_CONNECTION_MANAGER_H_

#include <base/macros.h>

#include "update_engine/dbus_wrapper_interface.h"

namespace chromeos_update_engine {

enum NetworkConnectionType {
  kNetEthernet = 0,
  kNetWifi,
  kNetWimax,
  kNetBluetooth,
  kNetCellular,
  kNetUnknown
};

enum class NetworkTethering {
  kNotDetected = 0,
  kSuspected,
  kConfirmed,
  kUnknown
};

class SystemState;

// This class exposes a generic interface to the connection manager
// (e.g FlimFlam, Shill, etc.) to consolidate all connection-related
// logic in update_engine.
class ConnectionManager {
 public:
  // Constructs a new ConnectionManager object initialized with the
  // given system state.
  explicit ConnectionManager(SystemState* system_state);

  // Populates |out_type| with the type of the network connection
  // that we are currently connected and |out_tethering| with the estimate of
  // whether that network is being tethered. The dbus_iface is used to query
  // the real connection manager (e.g shill).
  virtual bool GetConnectionProperties(DBusWrapperInterface* dbus_iface,
                                       NetworkConnectionType* out_type,
                                       NetworkTethering* out_tethering) const;

  // Returns true if we're allowed to update the system when we're
  // connected to the internet through the given network connection type and the
  // given tethering state.
  virtual bool IsUpdateAllowedOver(NetworkConnectionType type,
                                   NetworkTethering tethering) const;

  // Returns the string representation corresponding to the given
  // connection type.
  virtual const char* StringForConnectionType(NetworkConnectionType type) const;

  // Returns the string representation corresponding to the given tethering
  // state.
  virtual const char* StringForTethering(NetworkTethering tethering) const;

 private:
  // The global context for update_engine
  SystemState* system_state_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CONNECTION_MANAGER_H_
