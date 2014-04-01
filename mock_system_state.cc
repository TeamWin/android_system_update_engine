// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/mock_system_state.h"
#include "update_engine/policy_manager/fake_state.h"

using chromeos_policy_manager::FakeState;

namespace chromeos_update_engine {

// Mock the SystemStateInterface so that we could lie that
// OOBE is completed even when there's no such marker file, etc.
MockSystemState::MockSystemState()
  : default_request_params_(this),
    clock_(&default_clock_),
    hardware_(&default_hardware_),
    prefs_(&mock_prefs_),
    powerwash_safe_prefs_(&mock_powerwash_safe_prefs_),
    p2p_manager_(&mock_p2p_manager_),
    payload_state_(&mock_payload_state_),
    policy_manager_(&fake_policy_manager_) {
  request_params_ = &default_request_params_;
  mock_payload_state_.Initialize(this);
  mock_gpio_handler_ = new testing::NiceMock<MockGpioHandler>();
  mock_update_attempter_ = new testing::NiceMock<UpdateAttempterMock>(
      this, &dbus_);
  mock_connection_manager_ = new testing::NiceMock<MockConnectionManager>(this);
  connection_manager_ = mock_connection_manager_;
  fake_policy_manager_.Init(FakeState::Construct());
}

MockSystemState::~MockSystemState() {
  delete mock_connection_manager_;
  delete mock_update_attempter_;
  delete mock_gpio_handler_;
}

}  // namespace chromeos_update_engine
