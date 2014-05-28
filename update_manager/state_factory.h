// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_STATE_FACTORY_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_STATE_FACTORY_H_

#include "update_engine/dbus_wrapper_interface.h"
#include "update_engine/system_state.h"
#include "update_engine/update_manager/state.h"

namespace chromeos_update_manager {

// Creates and initializes a new UpdateManager State instance containing real
// providers instantiated using the passed interfaces. The State doesn't take
// ownership of the passed interfaces, which need to remain available during the
// life of this instance.  Returns null if one of the underlying providers fails
// to initialize.
State* DefaultStateFactory(
    policy::PolicyProvider* policy_provider,
    chromeos_update_engine::DBusWrapperInterface* dbus,
    chromeos_update_engine::SystemState* system_state);

}  // namespace chromeos_update_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_STATE_FACTORY_H_
