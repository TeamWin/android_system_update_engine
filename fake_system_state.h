// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_SYSTEM_STATE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_SYSTEM_STATE_H_

#include <base/logging.h>
#include <gmock/gmock.h>
#include <policy/mock_device_policy.h>

#include "metrics/metrics_library_mock.h"
#include "update_engine/fake_clock.h"
#include "update_engine/fake_hardware.h"
#include "update_engine/mock_connection_manager.h"
#include "update_engine/mock_dbus_wrapper.h"
#include "update_engine/mock_p2p_manager.h"
#include "update_engine/mock_payload_state.h"
#include "update_engine/policy_manager/fake_policy_manager.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/system_state.h"
#include "update_engine/update_attempter_mock.h"

namespace chromeos_update_engine {

// Mock the SystemStateInterface so that we could lie that
// OOBE is completed even when there's no such marker file, etc.
class FakeSystemState : public SystemState {
 public:
  FakeSystemState();

  // Base class overrides. All getters return the current implementation of
  // various members, either the default (fake/mock) or the one set to override
  // it by client code.

  virtual inline ClockInterface* clock() override {
    return clock_;
  }

  virtual inline void set_device_policy(
      const policy::DevicePolicy* device_policy) override {
    device_policy_ = device_policy;
  }

  virtual inline const policy::DevicePolicy* device_policy() override {
    return device_policy_;
  }

  virtual inline ConnectionManager* connection_manager() override {
    return connection_manager_;
  }

  virtual inline HardwareInterface* hardware() override {
    return hardware_;
  }

  virtual inline MetricsLibraryInterface* metrics_lib() override {
    return metrics_lib_;
  }

  virtual inline PrefsInterface* prefs() override {
    return prefs_;
  }

  virtual inline PrefsInterface* powerwash_safe_prefs() override {
    return powerwash_safe_prefs_;
  }

  virtual inline PayloadStateInterface* payload_state() override {
    return payload_state_;
  }

  virtual inline UpdateAttempter* update_attempter() override {
    return update_attempter_;
  }

  virtual inline OmahaRequestParams* request_params() override {
    return request_params_;
  }

  virtual inline P2PManager* p2p_manager() override {
    return p2p_manager_;
  }

  virtual inline chromeos_policy_manager::PolicyManager* policy_manager()
      override {
    return policy_manager_;
  }

  virtual inline bool system_rebooted() override {
    return fake_system_rebooted_;
  }

  // Setters for the various members, can be used for overriding the default
  // implementations. For convenience, setting to a null pointer will restore
  // the default implementation.

  inline void set_clock(ClockInterface* clock) {
    clock_ = clock ? clock : &fake_clock_;
  }

  inline void set_connection_manager(ConnectionManager* connection_manager) {
    connection_manager_ = (connection_manager ? connection_manager :
                           &mock_connection_manager_);
  }

  inline void set_hardware(HardwareInterface* hardware) {
    hardware_ = hardware ? hardware : &fake_hardware_;
  }

  inline void set_metrics_lib(MetricsLibraryInterface* metrics_lib) {
    metrics_lib_ = metrics_lib ? metrics_lib : &mock_metrics_lib_;
  }

  inline void set_prefs(PrefsInterface* prefs) {
    prefs_ = prefs ? prefs : &mock_prefs_;
  }

  inline void set_powerwash_safe_prefs(PrefsInterface* powerwash_safe_prefs) {
    powerwash_safe_prefs_ = (powerwash_safe_prefs ? powerwash_safe_prefs :
                             &mock_powerwash_safe_prefs_);
  }

  inline void set_payload_state(PayloadStateInterface *payload_state) {
    payload_state_ = payload_state ? payload_state : &mock_payload_state_;
  }

  inline void set_update_attempter(UpdateAttempter* update_attempter) {
    update_attempter_ = (update_attempter ? update_attempter :
                         &mock_update_attempter_);
  }

  inline void set_request_params(OmahaRequestParams* request_params) {
    request_params_ = (request_params ? request_params :
                       &default_request_params_);
  }

  inline void set_p2p_manager(P2PManager *p2p_manager) {
    p2p_manager_ = p2p_manager ? p2p_manager : &mock_p2p_manager_;
  }

  inline void set_policy_manager(
      chromeos_policy_manager::PolicyManager *policy_manager) {
    policy_manager_ = policy_manager ? policy_manager : &fake_policy_manager_;
  }

  inline void set_system_rebooted(bool system_rebooted) {
    fake_system_rebooted_ = system_rebooted;
  }

  // Getters for the built-in default implementations. These return the actual
  // concrete type of each implementation. For additional safety, they will fail
  // whenever the requested default was overridden by a different
  // implementation.

  inline FakeClock* fake_clock() {
    CHECK(clock_ == &fake_clock_);
    return &fake_clock_;
  }

  inline testing::NiceMock<MockConnectionManager>* mock_connection_manager() {
    CHECK(connection_manager_ == &mock_connection_manager_);
    return &mock_connection_manager_;
  }

  inline FakeHardware* fake_hardware() {
    CHECK(hardware_ == &fake_hardware_);
    return &fake_hardware_;
  }

  inline testing::NiceMock<MetricsLibraryMock>* mock_metrics_lib() {
    CHECK(metrics_lib_ == &mock_metrics_lib_);
    return &mock_metrics_lib_;
  }

  inline testing::NiceMock<PrefsMock> *mock_prefs() {
    CHECK(prefs_ == &mock_prefs_);
    return &mock_prefs_;
  }

  inline testing::NiceMock<PrefsMock> *mock_powerwash_safe_prefs() {
    CHECK(powerwash_safe_prefs_ == &mock_powerwash_safe_prefs_);
    return &mock_powerwash_safe_prefs_;
  }

  inline testing::NiceMock<MockPayloadState>* mock_payload_state() {
    CHECK(payload_state_ == &mock_payload_state_);
    return &mock_payload_state_;
  }

  inline testing::NiceMock<UpdateAttempterMock>* mock_update_attempter() {
    CHECK(update_attempter_ == &mock_update_attempter_);
    return &mock_update_attempter_;
  }

  inline OmahaRequestParams* default_request_params() {
    CHECK(request_params_ == &default_request_params_);
    return &default_request_params_;
  }

  inline testing::NiceMock<MockP2PManager>* mock_p2p_manager() {
    CHECK(p2p_manager_ == &mock_p2p_manager_);
    return &mock_p2p_manager_;
  }

  inline chromeos_policy_manager::FakePolicyManager* fake_policy_manager() {
    CHECK(policy_manager_ == &fake_policy_manager_);
    return &fake_policy_manager_;
  }

 private:
  // Default mock/fake implementations (owned).
  FakeClock fake_clock_;
  testing::NiceMock<MockConnectionManager> mock_connection_manager_;
  FakeHardware fake_hardware_;
  testing::NiceMock<MetricsLibraryMock> mock_metrics_lib_;
  testing::NiceMock<PrefsMock> mock_prefs_;
  testing::NiceMock<PrefsMock> mock_powerwash_safe_prefs_;
  testing::NiceMock<MockPayloadState> mock_payload_state_;
  testing::NiceMock<UpdateAttempterMock> mock_update_attempter_;
  OmahaRequestParams default_request_params_;
  testing::NiceMock<MockP2PManager> mock_p2p_manager_;
  chromeos_policy_manager::FakePolicyManager fake_policy_manager_;

  // Pointers to objects that client code can override. They are initialized to
  // the default implementations above.
  ClockInterface* clock_;
  ConnectionManager* connection_manager_;
  HardwareInterface* hardware_;
  MetricsLibraryInterface* metrics_lib_;
  PrefsInterface* prefs_;
  PrefsInterface* powerwash_safe_prefs_;
  PayloadStateInterface* payload_state_;
  UpdateAttempter* update_attempter_;
  OmahaRequestParams* request_params_;
  P2PManager* p2p_manager_;
  chromeos_policy_manager::PolicyManager* policy_manager_;

  // Other object pointers (not preinitialized).
  const policy::DevicePolicy* device_policy_;

  // Other data members.
  MockDBusWrapper dbus_;
  bool fake_system_rebooted_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_SYSTEM_STATE_H_
