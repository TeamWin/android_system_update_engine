// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__

#include <gmock/gmock.h>

#include <metrics/metrics_library_mock.h>
#include <policy/mock_device_policy.h>

#include "update_engine/mock_dbus_interface.h"
#include "update_engine/mock_gpio_handler.h"
#include "update_engine/mock_p2p_manager.h"
#include "update_engine/mock_payload_state.h"
#include "update_engine/clock.h"
#include "update_engine/mock_hardware.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/system_state.h"

namespace chromeos_update_engine {

class UpdateAttempterMock;

// Mock the SystemStateInterface so that we could lie that
// OOBE is completed even when there's no such marker file, etc.
class MockSystemState : public SystemState {
 public:
  MockSystemState();

  virtual ~MockSystemState();

  MOCK_METHOD0(IsOOBEComplete, bool());

  MOCK_METHOD1(set_device_policy, void(const policy::DevicePolicy*));
  MOCK_CONST_METHOD0(device_policy, const policy::DevicePolicy*());
  MOCK_METHOD0(system_rebooted, bool());

  inline virtual ClockInterface* clock() {
    return clock_;
  }

  inline virtual ConnectionManager* connection_manager() {
    return connection_manager_;
  }

  inline virtual HardwareInterface* hardware() {
    return hardware_;
  }

  inline virtual MetricsLibraryInterface* metrics_lib() {
    return &mock_metrics_lib_;
  }

  inline virtual PrefsInterface* prefs() {
    return prefs_;
  }

  inline virtual PrefsInterface* powerwash_safe_prefs() {
    return powerwash_safe_prefs_;
  }

  inline virtual PayloadStateInterface* payload_state() {
    return payload_state_;
  }

  inline virtual GpioHandler* gpio_handler() const {
    return mock_gpio_handler_;
  }

  virtual UpdateAttempter* update_attempter();

  inline virtual OmahaRequestParams* request_params() {
    return request_params_;
  }

  inline virtual P2PManager* p2p_manager() {
    return p2p_manager_;
  }

  // MockSystemState-specific public method.
  inline void set_connection_manager(ConnectionManager* connection_manager) {
    connection_manager_ = connection_manager;
  }

  inline MetricsLibraryMock* mock_metrics_lib() {
    return &mock_metrics_lib_;
  }

  inline void set_clock(ClockInterface* clock) {
    clock_ = clock;
  }

  inline void set_hardware(HardwareInterface* hardware) {
    hardware_ = hardware;
  }

  inline MockHardware& get_mock_hardware() {
    return default_hardware_;
  }

  inline void set_prefs(PrefsInterface* prefs) {
    prefs_ = prefs;
  }

  inline void set_powerwash_safe_prefs(PrefsInterface* prefs) {
    powerwash_safe_prefs_ = prefs;
  }

  inline testing::NiceMock<PrefsMock> *mock_prefs() {
    return &mock_prefs_;
  }

  inline testing::NiceMock<PrefsMock> *mock_powerwash_safe_prefs() {
    return &mock_powerwash_safe_prefs_;
  }

  inline MockPayloadState* mock_payload_state() {
    return &mock_payload_state_;
  }

  inline void set_request_params(OmahaRequestParams* params) {
    request_params_ = params;
  }

  inline void set_p2p_manager(P2PManager *p2p_manager) {
    p2p_manager_ = p2p_manager;
  }

  inline void set_payload_state(PayloadStateInterface *payload_state) {
    payload_state_ = payload_state;
  }

 private:
  // These are Mock objects or objects we own.
  testing::NiceMock<MetricsLibraryMock> mock_metrics_lib_;
  testing::NiceMock<PrefsMock> mock_prefs_;
  testing::NiceMock<PrefsMock> mock_powerwash_safe_prefs_;
  testing::NiceMock<MockP2PManager> mock_p2p_manager_;
  testing::NiceMock<MockPayloadState> mock_payload_state_;
  testing::NiceMock<MockGpioHandler>* mock_gpio_handler_;
  testing::NiceMock<UpdateAttempterMock>* mock_update_attempter_;
  MockDbusGlib dbus_;

  // These are the other object we own.
  Clock default_clock_;
  MockHardware default_hardware_;
  OmahaRequestParams default_request_params_;

  // These are pointers to objects which caller can override.
  ClockInterface* clock_;
  HardwareInterface* hardware_;
  PrefsInterface* prefs_;
  PrefsInterface* powerwash_safe_prefs_;
  ConnectionManager* connection_manager_;
  OmahaRequestParams* request_params_;
  P2PManager *p2p_manager_;
  PayloadStateInterface *payload_state_;
};

} // namespeace chromeos_update_engine

#endif // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
