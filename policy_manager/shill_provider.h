// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_SHILL_PROVIDER_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_SHILL_PROVIDER_H

#include <base/memory/scoped_ptr.h>
#include <base/time.h>

#include "update_engine/policy_manager/provider.h"
#include "update_engine/policy_manager/variable.h"

using base::Time;

namespace chromeos_policy_manager {

// TODO(garnold) Adapted from connection_manager.h.
enum ShillConnType {
  kShillConnTypeEthernet = 0,
  kShillConnTypeWifi,
  kShillConnTypeWimax,
  kShillConnTypeBluetooth,
  kShillConnTypeCellular,
  kShillConnTypeUnknown
};

// Provider for networking related information.
class ShillProvider : public Provider {
 public:
  // Returns whether we currently have network connectivity.
  Variable<bool>* var_is_connected() const {
    return var_is_connected_.get();
  }

  // Returns the current network connection type. Unknown if not connected.
  Variable<ShillConnType>* var_conn_type() const {
    return var_conn_type_.get();
  }

  // Returns the time when network connection last changed; initialized to
  // current time.
  Variable<base::Time>* var_conn_last_changed() const {
    return var_conn_last_changed_.get();
  }

 protected:
  ShillProvider() {}

  scoped_ptr<Variable<bool> > var_is_connected_;
  scoped_ptr<Variable<ShillConnType> > var_conn_type_;
  scoped_ptr<Variable<base::Time> > var_conn_last_changed_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_SHILL_PROVIDER_H
