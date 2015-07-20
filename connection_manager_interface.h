// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_CONNECTION_MANAGER_INTERFACE_H_
#define UPDATE_ENGINE_CONNECTION_MANAGER_INTERFACE_H_

#include <base/macros.h>

namespace chromeos_update_engine {

enum class NetworkConnectionType {
  kEthernet,
  kWifi,
  kWimax,
  kBluetooth,
  kCellular,
  kUnknown
};

enum class NetworkTethering {
  kNotDetected,
  kSuspected,
  kConfirmed,
  kUnknown
};

// This class exposes a generic interface to the connection manager
// (e.g FlimFlam, Shill, etc.) to consolidate all connection-related
// logic in update_engine.
class ConnectionManagerInterface {
 public:
  virtual ~ConnectionManagerInterface() = default;

  // Populates |out_type| with the type of the network connection
  // that we are currently connected and |out_tethering| with the estimate of
  // whether that network is being tethered.
  virtual bool GetConnectionProperties(NetworkConnectionType* out_type,
                                       NetworkTethering* out_tethering) = 0;

  // Returns true if we're allowed to update the system when we're
  // connected to the internet through the given network connection type and the
  // given tethering state.
  virtual bool IsUpdateAllowedOver(NetworkConnectionType type,
                                   NetworkTethering tethering) const = 0;

 protected:
  ConnectionManagerInterface() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionManagerInterface);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CONNECTION_MANAGER_INTERFACE_H_
