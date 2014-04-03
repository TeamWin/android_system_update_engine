// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_DEVICE_POLICY_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_DEVICE_POLICY_PROVIDER_H_

#include <set>
#include <string>

#include "update_engine/policy_manager/device_policy_provider.h"
#include "update_engine/policy_manager/fake_variable.h"

namespace chromeos_policy_manager {

// Fake implementation of the DevicePolicyProvider base class.
class FakeDevicePolicyProvider : public DevicePolicyProvider {
 public:
  FakeDevicePolicyProvider() {}

  virtual FakeVariable<bool>* var_device_policy_is_loaded() override {
    return &var_device_policy_is_loaded_;
  }

  virtual FakeVariable<std::string>* var_release_channel() override {
    return &var_release_channel_;
  }

  virtual FakeVariable<bool>* var_release_channel_delegated() override {
    return &var_release_channel_delegated_;
  }

  virtual FakeVariable<bool>* var_update_disabled() override {
    return &var_update_disabled_;
  }

  virtual FakeVariable<std::string>* var_target_version_prefix() override {
    return &var_target_version_prefix_;
  }

  virtual FakeVariable<base::TimeDelta>* var_scatter_factor() override {
    return &var_scatter_factor_;
  }

  virtual FakeVariable<std::set<ConnectionType>>*
      var_allowed_connection_types_for_update() override {
    return &var_allowed_connection_types_for_update_;
  }

  virtual FakeVariable<std::string>* var_get_owner() override {
    return &var_get_owner_;
  }

  virtual FakeVariable<bool>* var_http_downloads_enabled() override {
    return &var_http_downloads_enabled_;
  }

  virtual FakeVariable<bool>* var_au_p2p_enabled() override {
    return &var_au_p2p_enabled_;
  }

 private:
  virtual bool DoInit() override {
    return true;
  }

  FakeVariable<bool> var_device_policy_is_loaded_{
      "policy_is_loaded", kVariableModePoll};
  FakeVariable<std::string> var_release_channel_{
      "release_channel", kVariableModePoll};
  FakeVariable<bool> var_release_channel_delegated_{
      "release_channel_delegated", kVariableModePoll};
  FakeVariable<bool> var_update_disabled_{
      "update_disabled", kVariableModePoll};
  FakeVariable<std::string> var_target_version_prefix_{
      "target_version_prefix", kVariableModePoll};
  FakeVariable<base::TimeDelta> var_scatter_factor_{
      "scatter_factor", kVariableModePoll};
  FakeVariable<std::set<ConnectionType>>
      var_allowed_connection_types_for_update_{
          "allowed_connection_types_for_update", kVariableModePoll};
  FakeVariable<std::string> var_get_owner_{"get_owner", kVariableModePoll};
  FakeVariable<bool> var_http_downloads_enabled_{
      "http_downloads_enabled", kVariableModePoll};
  FakeVariable<bool> var_au_p2p_enabled_{"au_p2p_enabled", kVariableModePoll};

  DISALLOW_COPY_AND_ASSIGN(FakeDevicePolicyProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_DEVICE_POLICY_PROVIDER_H_
