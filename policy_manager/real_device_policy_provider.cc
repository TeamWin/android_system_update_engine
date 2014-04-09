// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_device_policy_provider.h"

#include <base/logging.h>
#include <base/time/time.h>
#include <policy/device_policy.h>

#include "update_engine/policy_manager/generic_variables.h"
#include "update_engine/policy_manager/real_shill_provider.h"
#include "update_engine/utils.h"

using base::TimeDelta;
using policy::DevicePolicy;
using std::set;
using std::string;

namespace {

const int kDevicePolicyRefreshRateInMinutes = 60;

}  // namespace

namespace chromeos_policy_manager {

RealDevicePolicyProvider::~RealDevicePolicyProvider() {
  CancelMainLoopEvent(scheduled_refresh_);
}

bool RealDevicePolicyProvider::DoInit() {
  CHECK(policy_provider_ != nullptr);

  // On Init() we try to get the device policy and keep updating it.
  RefreshDevicePolicyAndReschedule();

  return true;
}

void RealDevicePolicyProvider::RefreshDevicePolicyAndReschedule() {
  RefreshDevicePolicy();
  scheduled_refresh_ = RunFromMainLoopAfterTimeout(
      base::Bind(&RealDevicePolicyProvider::RefreshDevicePolicyAndReschedule,
                 base::Unretained(this)),
      TimeDelta::FromMinutes(kDevicePolicyRefreshRateInMinutes));
}

template<typename T>
void RealDevicePolicyProvider::UpdateVariable(
    AsyncCopyVariable<T>* var,
    bool (policy::DevicePolicy::*getter_method)(T*) const) {
  T new_value;
  if (policy_provider_->device_policy_is_loaded() &&
      (policy_provider_->GetDevicePolicy().*getter_method)(&new_value)) {
    var->SetValue(new_value);
  } else {
    var->UnsetValue();
  }
}

template<typename T>
void RealDevicePolicyProvider::UpdateVariable(
    AsyncCopyVariable<T>* var,
    bool (RealDevicePolicyProvider::*getter_method)(T*) const) {
  T new_value;
  if (policy_provider_->device_policy_is_loaded() &&
      (this->*getter_method)(&new_value)) {
    var->SetValue(new_value);
  } else {
    var->UnsetValue();
  }
}

bool RealDevicePolicyProvider::ConvertAllowedConnectionTypesForUpdate(
      set<ConnectionType>* allowed_types) const {
  set<string> allowed_types_str;
  if (!policy_provider_->GetDevicePolicy()
      .GetAllowedConnectionTypesForUpdate(&allowed_types_str)) {
    return false;
  }
  allowed_types->clear();
  for (auto& type_str : allowed_types_str) {
    ConnectionType type =
        RealShillProvider::ParseConnectionType(type_str.c_str());
    if (type != ConnectionType::kUnknown) {
      allowed_types->insert(type);
    } else {
      LOG(WARNING) << "Policy includes unknown connection type: " << type_str;
    }
  }
  return true;
}

bool RealDevicePolicyProvider::ConvertScatterFactor(
    base::TimeDelta* scatter_factor) const {
  int64 scatter_factor_in_seconds;
  if (!policy_provider_->GetDevicePolicy().GetScatterFactorInSeconds(
      &scatter_factor_in_seconds)) {
    return false;
  }
  if (scatter_factor_in_seconds < 0) {
    LOG(WARNING) << "Ignoring negative scatter factor: "
                 << scatter_factor_in_seconds;
    return false;
  }
  *scatter_factor = base::TimeDelta::FromSeconds(scatter_factor_in_seconds);
  return true;
}

void RealDevicePolicyProvider::RefreshDevicePolicy() {
  if (!policy_provider_->Reload()) {
    LOG(INFO) << "No device policies/settings present.";
  }

  var_device_policy_is_loaded_.SetValue(
      policy_provider_->device_policy_is_loaded());

  UpdateVariable(&var_release_channel_, &DevicePolicy::GetReleaseChannel);
  UpdateVariable(&var_release_channel_delegated_,
                 &DevicePolicy::GetReleaseChannelDelegated);
  UpdateVariable(&var_update_disabled_, &DevicePolicy::GetUpdateDisabled);
  UpdateVariable(&var_target_version_prefix_,
                 &DevicePolicy::GetTargetVersionPrefix);
  UpdateVariable(&var_scatter_factor_,
                 &RealDevicePolicyProvider::ConvertScatterFactor);
  UpdateVariable(
      &var_allowed_connection_types_for_update_,
      &RealDevicePolicyProvider::ConvertAllowedConnectionTypesForUpdate);
  UpdateVariable(&var_get_owner_, &DevicePolicy::GetOwner);
  UpdateVariable(&var_http_downloads_enabled_,
                 &DevicePolicy::GetHttpDownloadsEnabled);
  UpdateVariable(&var_au_p2p_enabled_, &DevicePolicy::GetAuP2PEnabled);
}

}  // namespace chromeos_policy_manager
