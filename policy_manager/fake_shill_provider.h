// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_SHILL_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_SHILL_PROVIDER_H_

#include "update_engine/policy_manager/fake_variable.h"
#include "update_engine/policy_manager/shill_provider.h"

namespace chromeos_policy_manager {

// Fake implementation of the ShillProvider base class.
class FakeShillProvider : public ShillProvider {
 public:
  FakeShillProvider() {}

 protected:
  virtual bool DoInit() {
    set_var_is_connected(
        new FakeVariable<bool>("is_connected", kVariableModePoll));
    set_var_conn_type(
        new FakeVariable<ConnectionType>("conn_type", kVariableModePoll));
    set_var_conn_last_changed(
        new FakeVariable<base::Time>("conn_last_changed", kVariableModePoll));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeShillProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_SHILL_PROVIDER_H_
