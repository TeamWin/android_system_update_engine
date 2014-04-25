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

  virtual FakeVariable<bool>* var_is_connected() override {
    return &var_is_connected_;
  }

  virtual FakeVariable<ConnectionType>* var_conn_type() override {
    return &var_conn_type_;
  }

  virtual FakeVariable<ConnectionTethering>*
      var_conn_tethering() override {
    return &var_conn_tethering_;
  }

  virtual FakeVariable<base::Time>* var_conn_last_changed() override {
    return &var_conn_last_changed_;
  }

 private:
  FakeVariable<bool> var_is_connected_{"is_connected", kVariableModePoll};
  FakeVariable<ConnectionType> var_conn_type_{"conn_type", kVariableModePoll};
  FakeVariable<ConnectionTethering> var_conn_tethering_{
      "conn_tethering", kVariableModePoll};
  FakeVariable<base::Time> var_conn_last_changed_{
      "conn_last_changed", kVariableModePoll};

  DISALLOW_COPY_AND_ASSIGN(FakeShillProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_SHILL_PROVIDER_H_
