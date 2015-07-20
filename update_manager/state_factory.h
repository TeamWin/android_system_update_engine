// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_STATE_FACTORY_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_STATE_FACTORY_H_

#include "update_engine/dbus_proxies.h"
#include "update_engine/shill_proxy.h"
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
    chromeos_update_engine::ShillProxy* shill_proxy,
    org::chromium::SessionManagerInterfaceProxyInterface* session_manager_proxy,
    chromeos_update_engine::SystemState* system_state);

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_STATE_FACTORY_H_
