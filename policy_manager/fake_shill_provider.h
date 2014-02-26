// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_SHILL_PROVIDER_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_SHILL_PROVIDER_H

#include "update_engine/policy_manager/fake_variable.h"
#include "update_engine/policy_manager/shill_provider.h"

namespace chromeos_policy_manager {

// Fake implementation of the ShillProvider base class.
class FakeShillProvider : public ShillProvider {
 public:
  FakeShillProvider() {}

 protected:
  virtual bool DoInit() {
    var_is_connected_.reset(
        new FakeVariable<bool>("is_connected", kVariableModeAsync));
    var_conn_type_.reset(
        new FakeVariable<ShillConnType>("conn_type", kVariableModeAsync));
    var_conn_last_changed_.reset(
        new FakeVariable<Time>("conn_last_changed", kVariableModeAsync));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeShillProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_SHILL_PROVIDER_H
