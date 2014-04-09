// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_SHILL_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_SHILL_PROVIDER_H_

#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>

#include "update_engine/policy_manager/provider.h"
#include "update_engine/policy_manager/variable.h"

namespace chromeos_policy_manager {

enum class ConnectionType {
  kEthernet,
  kWifi,
  kWimax,
  kBluetooth,
  kCellular,
  kUnknown
};

enum class ConnectionTethering {
  kNotDetected,
  kSuspected,
  kConfirmed,
  kUnknown,
};

// Provider for networking related information.
class ShillProvider : public Provider {
 public:
  // A variable returning whether we currently have network connectivity.
  virtual Variable<bool>* var_is_connected() = 0;

  // A variable returning the current network connection type. Unknown if not
  // connected.
  virtual Variable<ConnectionType>* var_conn_type() = 0;

  // A variable returning the tethering mode of a network connection. Unknown if
  // not connected.
  virtual Variable<ConnectionTethering>* var_conn_tethering() = 0;

  // A variable retruning the time when network connection last changed.
  // Initialized to current time.
  virtual Variable<base::Time>* var_conn_last_changed() = 0;

 protected:
  ShillProvider() {}
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_SHILL_PROVIDER_H_
