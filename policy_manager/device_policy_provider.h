// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_DEVICE_POLICY_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_DEVICE_POLICY_PROVIDER_H_

#include <set>
#include <string>

#include <policy/libpolicy.h>
#include <base/time/time.h>

#include "update_engine/policy_manager/provider.h"
#include "update_engine/policy_manager/shill_provider.h"
#include "update_engine/policy_manager/variable.h"

namespace chromeos_policy_manager {

// Provides access to the current DevicePolicy.
class DevicePolicyProvider : public Provider {
 public:
  virtual ~DevicePolicyProvider() {}

  // Variable stating whether the DevicePolicy was loaded.
  virtual Variable<bool>* var_device_policy_is_loaded() = 0;

  // Variables mapping the information received on the DevicePolicy protobuf.
  virtual Variable<std::string>* var_release_channel() = 0;

  virtual Variable<bool>* var_release_channel_delegated() = 0;

  virtual Variable<bool>* var_update_disabled() = 0;

  virtual Variable<std::string>* var_target_version_prefix() = 0;

  // Returns a non-negative scatter interval used for updates.
  virtual Variable<base::TimeDelta>* var_scatter_factor() = 0;

  // Variable returing the set of connection types allowed for updates. The
  // identifiers returned are consistent with the ones returned by the
  // ShillProvider.
  virtual Variable<std::set<ConnectionType>>*
      var_allowed_connection_types_for_update() = 0;

  // Variable stating the name of the device owner. For enterprise enrolled
  // devices, this will be an empty string.
  virtual Variable<std::string>* var_get_owner() = 0;

  virtual Variable<bool>* var_http_downloads_enabled() = 0;

  virtual Variable<bool>* var_au_p2p_enabled() = 0;

 protected:
  DevicePolicyProvider() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DevicePolicyProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_DEVICE_POLICY_PROVIDER_H_
