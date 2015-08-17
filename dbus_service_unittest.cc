// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/dbus_service.h"

#include <gtest/gtest.h>
#include <string>

#include <chromeos/errors/error.h>
#include <policy/libpolicy.h>
#include <policy/mock_device_policy.h>

#include "update_engine/dbus_constants.h"
#include "update_engine/fake_system_state.h"

using std::string;
using testing::Return;
using testing::SetArgumentPointee;
using testing::_;

using chromeos::errors::dbus::kDomain;

namespace chromeos_update_engine {

class UpdateEngineServiceTest : public ::testing::Test {
 protected:
  UpdateEngineServiceTest()
      : mock_update_attempter_(fake_system_state_.mock_update_attempter()),
        dbus_service_(&fake_system_state_) {}

  void SetUp() override {
    fake_system_state_.set_device_policy(nullptr);
  }

  // Fake/mock infrastructure.
  FakeSystemState fake_system_state_;
  policy::MockDevicePolicy mock_device_policy_;

  // Shortcut for fake_system_state_.mock_update_attempter().
  MockUpdateAttempter* mock_update_attempter_;

  chromeos::ErrorPtr error_;
  UpdateEngineService dbus_service_;
};

TEST_F(UpdateEngineServiceTest, AttemptUpdate) {
  // Simple test to ensure that the default is an interactive check.
  EXPECT_CALL(*mock_update_attempter_,
              CheckForUpdate("app_ver", "url", true /* interactive */));
  EXPECT_TRUE(dbus_service_.AttemptUpdate(&error_, "app_ver", "url"));
  EXPECT_EQ(nullptr, error_);
}

TEST_F(UpdateEngineServiceTest, AttemptUpdateWithFlags) {
  EXPECT_CALL(*mock_update_attempter_, CheckForUpdate(
      "app_ver", "url", false /* interactive */));
  // The update is non-interactive when we pass the non-interactive flag.
  EXPECT_TRUE(dbus_service_.AttemptUpdateWithFlags(
      &error_, "app_ver", "url", kAttemptUpdateFlagNonInteractive));
  EXPECT_EQ(nullptr, error_);
}

// SetChannel is allowed when there's no device policy (the device is not
// enterprise enrolled).
TEST_F(UpdateEngineServiceTest, SetChannelWithNoPolicy) {
  EXPECT_CALL(*mock_update_attempter_, RefreshDevicePolicy());
  // If SetTargetChannel is called it means the policy check passed.
  EXPECT_CALL(*fake_system_state_.mock_request_params(),
              SetTargetChannel("stable-channel", true))
      .WillOnce(Return(true));
  EXPECT_TRUE(dbus_service_.SetChannel(&error_, "stable-channel", true));
  ASSERT_EQ(nullptr, error_);
}

// When the policy is present, the delegated value should be checked.
TEST_F(UpdateEngineServiceTest, SetChannelWithDelegatedPolicy) {
  policy::MockDevicePolicy mock_device_policy;
  fake_system_state_.set_device_policy(&mock_device_policy);
  EXPECT_CALL(mock_device_policy, GetReleaseChannelDelegated(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*fake_system_state_.mock_request_params(),
              SetTargetChannel("beta-channel", true))
      .WillOnce(Return(true));

  EXPECT_TRUE(dbus_service_.SetChannel(&error_, "beta-channel", true));
  ASSERT_EQ(nullptr, error_);
}

// When passing an invalid value (SetTargetChannel fails) an error should be
// raised.
TEST_F(UpdateEngineServiceTest, SetChannelWithInvalidChannel) {
  EXPECT_CALL(*mock_update_attempter_, RefreshDevicePolicy());
  EXPECT_CALL(*fake_system_state_.mock_request_params(),
              SetTargetChannel("foo-channel", true)).WillOnce(Return(false));

  EXPECT_FALSE(dbus_service_.SetChannel(&error_, "foo-channel", true));
  ASSERT_NE(nullptr, error_);
  EXPECT_TRUE(error_->HasError(kDomain, kUpdateEngineServiceErrorFailed));
}

TEST_F(UpdateEngineServiceTest, GetChannel) {
  fake_system_state_.mock_request_params()->set_current_channel("current");
  fake_system_state_.mock_request_params()->set_target_channel("target");
  string channel;
  EXPECT_TRUE(dbus_service_.GetChannel(
      &error_, true /* get_current_channel */, &channel));
  EXPECT_EQ(nullptr, error_);
  EXPECT_EQ("current", channel);

  EXPECT_TRUE(dbus_service_.GetChannel(
      &error_, false /* get_current_channel */, &channel));
  EXPECT_EQ(nullptr, error_);
  EXPECT_EQ("target", channel);
}

TEST_F(UpdateEngineServiceTest, ResetStatusSucceeds) {
  EXPECT_CALL(*mock_update_attempter_, ResetStatus()).WillOnce(Return(true));
  EXPECT_TRUE(dbus_service_.ResetStatus(&error_));
  EXPECT_EQ(nullptr, error_);
}

TEST_F(UpdateEngineServiceTest, ResetStatusFails) {
  EXPECT_CALL(*mock_update_attempter_, ResetStatus()).WillOnce(Return(false));
  EXPECT_FALSE(dbus_service_.ResetStatus(&error_));
  ASSERT_NE(nullptr, error_);
  EXPECT_TRUE(error_->HasError(kDomain, kUpdateEngineServiceErrorFailed));
}

}  // namespace chromeos_update_engine
