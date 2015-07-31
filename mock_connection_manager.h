// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_MOCK_CONNECTION_MANAGER_H_
#define UPDATE_ENGINE_MOCK_CONNECTION_MANAGER_H_

#include <gmock/gmock.h>

#include "update_engine/connection_manager_interface.h"

namespace chromeos_update_engine {

// This class mocks the generic interface to the connection manager
// (e.g FlimFlam, Shill, etc.) to consolidate all connection-related
// logic in update_engine.
class MockConnectionManager : public ConnectionManagerInterface {
 public:
  MockConnectionManager() = default;

  MOCK_CONST_METHOD3(GetConnectionProperties,
                     bool(DBusWrapperInterface* dbus_iface,
                          NetworkConnectionType* out_type,
                          NetworkTethering* out_tethering));

  MOCK_CONST_METHOD2(IsUpdateAllowedOver, bool(NetworkConnectionType type,
                                               NetworkTethering tethering));
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_MOCK_CONNECTION_MANAGER_H_
