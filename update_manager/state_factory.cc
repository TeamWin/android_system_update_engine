// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/state_factory.h"

#include <memory>

#include <base/logging.h>

#include "update_engine/clock_interface.h"
#include "update_engine/update_manager/real_config_provider.h"
#include "update_engine/update_manager/real_device_policy_provider.h"
#include "update_engine/update_manager/real_random_provider.h"
#include "update_engine/update_manager/real_shill_provider.h"
#include "update_engine/update_manager/real_state.h"
#include "update_engine/update_manager/real_system_provider.h"
#include "update_engine/update_manager/real_time_provider.h"
#include "update_engine/update_manager/real_updater_provider.h"

using std::unique_ptr;

namespace chromeos_update_manager {

State* DefaultStateFactory(
    policy::PolicyProvider* policy_provider,
    chromeos_update_engine::ShillProxy* shill_proxy,
    org::chromium::SessionManagerInterfaceProxyInterface* session_manager_proxy,
    chromeos_update_engine::SystemState* system_state) {
  chromeos_update_engine::ClockInterface* const clock = system_state->clock();
  unique_ptr<RealConfigProvider> config_provider(
      new RealConfigProvider(system_state->hardware()));
  unique_ptr<RealDevicePolicyProvider> device_policy_provider(
      new RealDevicePolicyProvider(session_manager_proxy, policy_provider));
  unique_ptr<RealRandomProvider> random_provider(new RealRandomProvider());
  unique_ptr<RealShillProvider> shill_provider(
      new RealShillProvider(shill_proxy, clock));
  unique_ptr<RealSystemProvider> system_provider(
      new RealSystemProvider(system_state->hardware()));
  unique_ptr<RealTimeProvider> time_provider(new RealTimeProvider(clock));
  unique_ptr<RealUpdaterProvider> updater_provider(
      new RealUpdaterProvider(system_state));

  if (!(config_provider->Init() &&
        device_policy_provider->Init() &&
        random_provider->Init() &&
        shill_provider->Init() &&
        system_provider->Init() &&
        time_provider->Init() &&
        updater_provider->Init())) {
    LOG(ERROR) << "Error initializing providers";
    return nullptr;
  }

  return new RealState(config_provider.release(),
                       device_policy_provider.release(),
                       random_provider.release(),
                       shill_provider.release(),
                       system_provider.release(),
                       time_provider.release(),
                       updater_provider.release());
}

}  // namespace chromeos_update_manager
