// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SHILL_PROVIDER_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SHILL_PROVIDER_H

#include "update_engine/policy_manager/shill_provider.h"

using base::Time;

namespace chromeos_policy_manager {

// ShillProvider concrete implementation.
//
// TODO(garnold) Much of the functionality in this module was adapted from
// connection_manager, with slight changes (annotated inline).  We need to make
// sure to deprecate use of connection manager when the time comes.
class RealShillProvider : public ShillProvider {
 public:
  // TODO(garnold) This should take a DBus object for communicating with shill.
  RealShillProvider()
      : is_connected_(false), conn_type_(kShillConnTypeUnknown),
        conn_last_changed_(Time::Now()) {}

 protected:
  virtual bool DoInit();

 private:
  // Whether we have network connectivity.
  bool is_connected_;

  // The current network connection type as reported by shill.
  ShillConnType conn_type_;

  // The time when the connection type last changed.
  Time conn_last_changed_;

  DISALLOW_COPY_AND_ASSIGN(RealShillProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SHILL_PROVIDER_H
