// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/state_factory.h"

#include <base/logging.h>

#include "update_engine/policy_manager/real_random_provider.h"
#include "update_engine/policy_manager/real_shill_provider.h"
#include "update_engine/policy_manager/real_system_provider.h"
#include "update_engine/policy_manager/real_time_provider.h"

namespace chromeos_policy_manager {

State* DefaultStateFactory(
    chromeos_update_engine::DBusWrapperInterface* dbus,
    chromeos_update_engine::ClockInterface* clock) {
  scoped_ptr<RealRandomProvider> random_provider(new RealRandomProvider());
  scoped_ptr<RealShillProvider> shill_provider(
      new RealShillProvider(dbus, clock));
  scoped_ptr<RealSystemProvider> system_provider(new RealSystemProvider());
  scoped_ptr<RealTimeProvider> time_provider(new RealTimeProvider(clock));

  if (!(random_provider->Init() &&
        shill_provider->Init() &&
        system_provider->Init() &&
        time_provider->Init())) {
    LOG(ERROR) << "Error initializing providers";
    return NULL;
  }

  return new State(random_provider.release(),
                   shill_provider.release(),
                   system_provider.release(),
                   time_provider.release());
}

}  // namespace chromeos_policy_manager
