// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_REAL_DEVICE_POLICY_PROVIDER_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_REAL_DEVICE_POLICY_PROVIDER_H_

#include <set>
#include <string>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <policy/libpolicy.h>

#include "update_engine/update_manager/device_policy_provider.h"
#include "update_engine/update_manager/event_loop.h"
#include "update_engine/update_manager/generic_variables.h"

namespace chromeos_update_manager {

// DevicePolicyProvider concrete implementation.
class RealDevicePolicyProvider : public DevicePolicyProvider {
 public:
  explicit RealDevicePolicyProvider(policy::PolicyProvider* policy_provider)
      : policy_provider_(policy_provider) {}
  ~RealDevicePolicyProvider();

  // Initializes the provider and returns whether it succeeded.
  bool Init();

  virtual Variable<bool>* var_device_policy_is_loaded() override {
    return &var_device_policy_is_loaded_;
  }

  virtual Variable<std::string>* var_release_channel() override {
    return &var_release_channel_;
  }

  virtual Variable<bool>* var_release_channel_delegated() override {
    return &var_release_channel_delegated_;
  }

  virtual Variable<bool>* var_update_disabled() override {
    return &var_update_disabled_;
  }

  virtual Variable<std::string>* var_target_version_prefix() override {
    return &var_target_version_prefix_;
  }

  virtual Variable<base::TimeDelta>* var_scatter_factor() override {
    return &var_scatter_factor_;
  }

  virtual Variable<std::set<ConnectionType>>*
      var_allowed_connection_types_for_update() override {
    return &var_allowed_connection_types_for_update_;
  }

  virtual Variable<std::string>* var_get_owner() override {
    return &var_get_owner_;
  }

  virtual Variable<bool>* var_http_downloads_enabled() override {
    return &var_http_downloads_enabled_;
  }

  virtual Variable<bool>* var_au_p2p_enabled() override {
    return &var_au_p2p_enabled_;
  }

 private:
  FRIEND_TEST(UmRealDevicePolicyProviderTest, RefreshScheduledTest);
  FRIEND_TEST(UmRealDevicePolicyProviderTest, NonExistentDevicePolicyReloaded);
  FRIEND_TEST(UmRealDevicePolicyProviderTest, ValuesUpdated);

  // Schedules a call to periodically refresh the device policy.
  void RefreshDevicePolicyAndReschedule();

  // Reloads the device policy and updates all the exposed variables.
  void RefreshDevicePolicy();

  // Updates the async variable |var| based on the result value of the method
  // passed, which is a DevicePolicy getter method.
  template<typename T>
  void UpdateVariable(AsyncCopyVariable<T>* var,
                      bool (policy::DevicePolicy::*getter_method)(T*) const);

  // Updates the async variable |var| based on the result value of the getter
  // method passed, which is a wrapper getter on this class.
  template<typename T>
  void UpdateVariable(
      AsyncCopyVariable<T>* var,
      bool (RealDevicePolicyProvider::*getter_method)(T*) const);

  // Wrapper for DevicePolicy::GetScatterFactorInSeconds() that converts the
  // result to a base::TimeDelta. It returns the same value as
  // GetScatterFactorInSeconds().
  bool ConvertScatterFactor(base::TimeDelta* scatter_factor) const;

  // Wrapper for DevicePolicy::GetAllowedConnectionTypesForUpdate() that
  // converts the result to a set of ConnectionType elements instead of strings.
  bool ConvertAllowedConnectionTypesForUpdate(
      std::set<ConnectionType>* allowed_types) const;

  // Used for fetching information about the device policy.
  policy::PolicyProvider* policy_provider_;

  // Used to schedule refreshes of the device policy.
  EventId scheduled_refresh_ = kEventIdNull;

  // Variable exposing whether the policy is loaded.
  AsyncCopyVariable<bool> var_device_policy_is_loaded_{
      "policy_is_loaded", false};

  // Variables mapping the exposed methods from the policy::DevicePolicy.
  AsyncCopyVariable<std::string> var_release_channel_{"release_channel"};
  AsyncCopyVariable<bool> var_release_channel_delegated_{
      "release_channel_delegated"};
  AsyncCopyVariable<bool> var_update_disabled_{"update_disabled"};
  AsyncCopyVariable<std::string> var_target_version_prefix_{
      "target_version_prefix"};
  AsyncCopyVariable<base::TimeDelta> var_scatter_factor_{"scatter_factor"};
  AsyncCopyVariable<std::set<ConnectionType>>
      var_allowed_connection_types_for_update_{
          "allowed_connection_types_for_update"};
  AsyncCopyVariable<std::string> var_get_owner_{"get_owner"};
  AsyncCopyVariable<bool> var_http_downloads_enabled_{"http_downloads_enabled"};
  AsyncCopyVariable<bool> var_au_p2p_enabled_{"au_p2p_enabled"};

  DISALLOW_COPY_AND_ASSIGN(RealDevicePolicyProvider);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_REAL_DEVICE_POLICY_PROVIDER_H_
