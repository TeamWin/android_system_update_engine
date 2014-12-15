// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/real_device_policy_provider.h"

#include <memory>

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <policy/mock_device_policy.h>
#include <policy/mock_libpolicy.h>

#include "update_engine/mock_dbus_wrapper.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_manager/umtest_utils.h"

using base::TimeDelta;
using chromeos_update_engine::test_utils::RunGMainLoopMaxIterations;
using std::set;
using std::string;
using std::unique_ptr;
using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::_;

namespace {

// Fake dbus-glib objects. These should be different values, to ease diagnosis
// of errors.
DBusGConnection* const kFakeConnection = reinterpret_cast<DBusGConnection*>(1);
DBusGProxy* const kFakeManagerProxy = reinterpret_cast<DBusGProxy*>(2);

}  // namespace

namespace chromeos_update_manager {

class UmRealDevicePolicyProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    provider_.reset(new RealDevicePolicyProvider(&mock_dbus_,
                                                 &mock_policy_provider_));
    // By default, we have a device policy loaded. Tests can call
    // SetUpNonExistentDevicePolicy() to override this.
    SetUpExistentDevicePolicy();

    SetUpDBusSignalExpectations();
  }

  void TearDown() override {
    // Check for leaked callbacks on the main loop.
    EXPECT_EQ(0, RunGMainLoopMaxIterations(100));

    // We need to set these expectation before the object is destroyed but
    // after it finished running the test so the values of signal_callback_ and
    // signal_callback_data_ are correct.
    EXPECT_CALL(mock_dbus_, ProxyDisconnectSignal(
            kFakeManagerProxy,
            StrEq(login_manager::kPropertyChangeCompleteSignal),
            signal_callback_,
            signal_callback_data_));
    EXPECT_CALL(mock_dbus_, ProxyUnref(kFakeManagerProxy));

    provider_.reset();
  }

  void SetUpNonExistentDevicePolicy() {
    ON_CALL(mock_policy_provider_, Reload())
        .WillByDefault(Return(false));
    ON_CALL(mock_policy_provider_, device_policy_is_loaded())
        .WillByDefault(Return(false));
    EXPECT_CALL(mock_policy_provider_, GetDevicePolicy()).Times(0);
  }

  void SetUpExistentDevicePolicy() {
    // Setup the default behavior of the mocked PolicyProvider.
    ON_CALL(mock_policy_provider_, Reload())
        .WillByDefault(Return(true));
    ON_CALL(mock_policy_provider_, device_policy_is_loaded())
        .WillByDefault(Return(true));
    ON_CALL(mock_policy_provider_, GetDevicePolicy())
        .WillByDefault(ReturnRef(mock_device_policy_));
  }

  void SetUpDBusSignalExpectations() {
    // Setup the DBus connection with default actions that should be performed
    // once.
    EXPECT_CALL(mock_dbus_, BusGet(_, _)).WillOnce(
        Return(kFakeConnection));
    EXPECT_CALL(mock_dbus_, ProxyNewForName(
            kFakeConnection, StrEq(login_manager::kSessionManagerServiceName),
            StrEq(login_manager::kSessionManagerServicePath),
            StrEq(login_manager::kSessionManagerInterface)))
        .WillOnce(Return(kFakeManagerProxy));

    // Expect the signal to be added, registered and released.
    EXPECT_CALL(mock_dbus_, ProxyAddSignal_1(
            kFakeManagerProxy,
            StrEq(login_manager::kPropertyChangeCompleteSignal),
            G_TYPE_STRING));
    EXPECT_CALL(mock_dbus_, ProxyConnectSignal(
            kFakeManagerProxy,
            StrEq(login_manager::kPropertyChangeCompleteSignal),
            _ /* callback */, _ /* data */, _ /* free function */))
        .WillOnce(DoAll(SaveArg<2>(&signal_callback_),
                        SaveArg<3>(&signal_callback_data_)));
  }

  chromeos_update_engine::MockDBusWrapper mock_dbus_;
  testing::NiceMock<policy::MockDevicePolicy> mock_device_policy_;
  testing::NiceMock<policy::MockPolicyProvider> mock_policy_provider_;
  unique_ptr<RealDevicePolicyProvider> provider_;

  // The registered signal handler for the signal.
  GCallback signal_callback_ = nullptr;
  void* signal_callback_data_ = nullptr;
};

TEST_F(UmRealDevicePolicyProviderTest, RefreshScheduledTest) {
  // Check that the RefreshPolicy gets scheduled by checking the EventId.
  EXPECT_TRUE(provider_->Init());
  EXPECT_NE(kEventIdNull, provider_->scheduled_refresh_);
}

TEST_F(UmRealDevicePolicyProviderTest, FirstReload) {
  // Checks that the policy is reloaded and the DevicePolicy is consulted.
  EXPECT_CALL(mock_policy_provider_, Reload());
  EXPECT_TRUE(provider_->Init());
}

TEST_F(UmRealDevicePolicyProviderTest, NonExistentDevicePolicyReloaded) {
  // Checks that the policy is reloaded by RefreshDevicePolicy().
  SetUpNonExistentDevicePolicy();
  EXPECT_CALL(mock_policy_provider_, Reload()).Times(2);
  EXPECT_TRUE(provider_->Init());
  // Force the policy refresh.
  provider_->RefreshDevicePolicy();
}

TEST_F(UmRealDevicePolicyProviderTest, SessionManagerSignalForcesReload) {
  // Checks that a signal from the SessionManager forces a reload.
  SetUpNonExistentDevicePolicy();
  EXPECT_CALL(mock_policy_provider_, Reload()).Times(2);
  EXPECT_TRUE(provider_->Init());

  ASSERT_NE(nullptr, signal_callback_);
  // Convert the GCallback to a function pointer and call it. GCallback is just
  // a void function pointer to ensure that the type of the passed callback is a
  // pointer. We need to cast it back to the right function type before calling
  // it.
  typedef void (*StaticSignalHandler)(DBusGProxy*, const char*, void*);
  StaticSignalHandler signal_handler = reinterpret_cast<StaticSignalHandler>(
      signal_callback_);
  (*signal_handler)(kFakeManagerProxy, "success", signal_callback_data_);
}

TEST_F(UmRealDevicePolicyProviderTest, NonExistentDevicePolicyEmptyVariables) {
  SetUpNonExistentDevicePolicy();
  EXPECT_CALL(mock_policy_provider_, GetDevicePolicy()).Times(0);
  EXPECT_TRUE(provider_->Init());

  UmTestUtils::ExpectVariableHasValue(false,
                                      provider_->var_device_policy_is_loaded());

  UmTestUtils::ExpectVariableNotSet(provider_->var_release_channel());
  UmTestUtils::ExpectVariableNotSet(provider_->var_release_channel_delegated());
  UmTestUtils::ExpectVariableNotSet(provider_->var_update_disabled());
  UmTestUtils::ExpectVariableNotSet(provider_->var_target_version_prefix());
  UmTestUtils::ExpectVariableNotSet(provider_->var_scatter_factor());
  UmTestUtils::ExpectVariableNotSet(
      provider_->var_allowed_connection_types_for_update());
  UmTestUtils::ExpectVariableNotSet(provider_->var_owner());
  UmTestUtils::ExpectVariableNotSet(provider_->var_http_downloads_enabled());
  UmTestUtils::ExpectVariableNotSet(provider_->var_au_p2p_enabled());
}

TEST_F(UmRealDevicePolicyProviderTest, ValuesUpdated) {
  SetUpNonExistentDevicePolicy();
  EXPECT_TRUE(provider_->Init());
  Mock::VerifyAndClearExpectations(&mock_policy_provider_);

  // Reload the policy with a good one and set some values as present. The
  // remaining values are false.
  SetUpExistentDevicePolicy();
  EXPECT_CALL(mock_device_policy_, GetReleaseChannel(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(string("mychannel")),
                      Return(true)));
  EXPECT_CALL(mock_device_policy_, GetAllowedConnectionTypesForUpdate(_))
      .WillOnce(Return(false));

  provider_->RefreshDevicePolicy();

  UmTestUtils::ExpectVariableHasValue(true,
                                      provider_->var_device_policy_is_loaded());

  // Test that at least one variable is set, to ensure the refresh occurred.
  UmTestUtils::ExpectVariableHasValue(string("mychannel"),
                                      provider_->var_release_channel());
  UmTestUtils::ExpectVariableNotSet(
      provider_->var_allowed_connection_types_for_update());
}

TEST_F(UmRealDevicePolicyProviderTest, ScatterFactorConverted) {
  SetUpExistentDevicePolicy();
  EXPECT_CALL(mock_device_policy_, GetScatterFactorInSeconds(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(1234), Return(true)));
  EXPECT_TRUE(provider_->Init());

  UmTestUtils::ExpectVariableHasValue(TimeDelta::FromSeconds(1234),
                                      provider_->var_scatter_factor());
}

TEST_F(UmRealDevicePolicyProviderTest, NegativeScatterFactorIgnored) {
  SetUpExistentDevicePolicy();
  EXPECT_CALL(mock_device_policy_, GetScatterFactorInSeconds(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(-1), Return(true)));
  EXPECT_TRUE(provider_->Init());

  UmTestUtils::ExpectVariableNotSet(provider_->var_scatter_factor());
}

TEST_F(UmRealDevicePolicyProviderTest, AllowedTypesConverted) {
  SetUpExistentDevicePolicy();
  EXPECT_CALL(mock_device_policy_, GetAllowedConnectionTypesForUpdate(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(
                          set<string>{"bluetooth", "wifi", "not-a-type"}),
                      Return(true)));
  EXPECT_TRUE(provider_->Init());

  UmTestUtils::ExpectVariableHasValue(
      set<ConnectionType>{ConnectionType::kWifi, ConnectionType::kBluetooth},
      provider_->var_allowed_connection_types_for_update());
}

}  // namespace chromeos_update_manager
