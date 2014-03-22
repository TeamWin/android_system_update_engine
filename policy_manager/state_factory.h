// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_FACTORY_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_FACTORY_H_

#include "update_engine/clock_interface.h"
#include "update_engine/dbus_wrapper_interface.h"
#include "update_engine/policy_manager/state.h"

namespace chromeos_policy_manager {

// Creates and initializes a new State instance using the real providers
// instantiated using the passed interfaces when needed. The State doesn't
// take ownership of the passed interfaces, which need to remain available
// during the life of this instance.
// If one of the underlying providers fails to initialize, this function returns
// NULL.
State* DefaultStateFactory(
    chromeos_update_engine::DBusWrapperInterface* dbus,
    chromeos_update_engine::ClockInterface* clock);

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_FACTORY_H_
