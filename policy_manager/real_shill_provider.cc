// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/generic_variables.h"
#include "update_engine/policy_manager/real_shill_provider.h"

namespace chromeos_policy_manager {

// ShillProvider implementation.

bool RealShillProvider::DoInit(void) {
  // TODO(garnold) Initialize with actual (or fake) DBus connection.

  var_is_connected_.reset(
      new CopyVariable<bool>("is_connected", kVariableModeAsync,
                             is_connected_));
  var_conn_type_.reset(
      new CopyVariable<ShillConnType>("conn_type", kVariableModeAsync,
                                      conn_type_));
  var_conn_last_changed_.reset(
      new CopyVariable<Time>("conn_last_changed", kVariableModeAsync,
                             conn_last_changed_));
  return true;
}

}  // namespace chromeos_policy_manager
