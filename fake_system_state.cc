// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/fake_system_state.h"
#include "update_engine/update_manager/fake_state.h"

using chromeos_update_manager::FakeState;

namespace chromeos_update_engine {

// Mock the SystemStateInterface so that we could lie that
// OOBE is completed even when there's no such marker file, etc.
FakeSystemState::FakeSystemState()
  : mock_connection_manager_(this),
    mock_update_attempter_(this, &dbus_),
    default_request_params_(this),
    fake_update_manager_(&fake_clock_),
    clock_(&fake_clock_),
    connection_manager_(&mock_connection_manager_),
    hardware_(&fake_hardware_),
    metrics_lib_(&mock_metrics_lib_),
    prefs_(&mock_prefs_),
    powerwash_safe_prefs_(&mock_powerwash_safe_prefs_),
    payload_state_(&mock_payload_state_),
    update_attempter_(&mock_update_attempter_),
    request_params_(&default_request_params_),
    p2p_manager_(&mock_p2p_manager_),
    update_manager_(&fake_update_manager_),
    device_policy_(nullptr),
    fake_system_rebooted_(false) {
  mock_payload_state_.Initialize(this);
  mock_update_attempter_.Init();
}

}  // namespace chromeos_update_engine
