// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_device_policy_provider.h"

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>
#include <policy/mock_device_policy.h>
#include <policy/mock_libpolicy.h>

#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/test_utils.h"

using base::TimeDelta;
using chromeos_update_engine::RunGMainLoopMaxIterations;
using std::set;
using std::string;
using testing::AtLeast;
using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgumentPointee;
using testing::_;

namespace chromeos_policy_manager {

class PmRealDevicePolicyProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    provider_.reset(new RealDevicePolicyProvider(&mock_policy_provider_));
    // By default, we have a device policy loaded. Tests can call
    // SetUpNonExistentDevicePolicy() to override this.
    SetUpExistentDevicePolicy();
  }

  virtual void TearDown() {
    // Check for leaked callbacks on the main loop.
    EXPECT_EQ(0, RunGMainLoopMaxIterations(100));
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

  testing::NiceMock<policy::MockDevicePolicy> mock_device_policy_;
  testing::NiceMock<policy::MockPolicyProvider> mock_policy_provider_;
  scoped_ptr<RealDevicePolicyProvider> provider_;
};

TEST_F(PmRealDevicePolicyProviderTest, RefreshScheduledTest) {
  // Check that the RefreshPolicy gets scheduled by checking the EventId.
  EXPECT_TRUE(provider_->Init());
  EXPECT_NE(kEventIdNull, provider_->scheduled_refresh_);
}

TEST_F(PmRealDevicePolicyProviderTest, FirstReload) {
  // Checks that the policy is reloaded and the DevicePolicy is consulted.
  EXPECT_CALL(mock_policy_provider_, Reload());
  EXPECT_TRUE(provider_->Init());
}

TEST_F(PmRealDevicePolicyProviderTest, NonExistentDevicePolicyReloaded) {
  // Checks that the policy is reloaded by RefreshDevicePolicy().
  SetUpNonExistentDevicePolicy();
  EXPECT_CALL(mock_policy_provider_, Reload()).Times(2);
  EXPECT_TRUE(provider_->Init());
  // Force the policy refresh.
  provider_->RefreshDevicePolicy();
}

TEST_F(PmRealDevicePolicyProviderTest, NonExistentDevicePolicyEmptyVariables) {
  SetUpNonExistentDevicePolicy();
  EXPECT_CALL(mock_policy_provider_, GetDevicePolicy()).Times(0);
  EXPECT_TRUE(provider_->Init());

  PmTestUtils::ExpectVariableHasValue(false,
                                      provider_->var_device_policy_is_loaded());

  PmTestUtils::ExpectVariableNotSet(provider_->var_release_channel());
  PmTestUtils::ExpectVariableNotSet(provider_->var_release_channel_delegated());
  PmTestUtils::ExpectVariableNotSet(provider_->var_update_disabled());
  PmTestUtils::ExpectVariableNotSet(provider_->var_target_version_prefix());
  PmTestUtils::ExpectVariableNotSet(provider_->var_scatter_factor());
  PmTestUtils::ExpectVariableNotSet(
      provider_->var_allowed_connection_types_for_update());
  PmTestUtils::ExpectVariableNotSet(provider_->var_get_owner());
  PmTestUtils::ExpectVariableNotSet(provider_->var_http_downloads_enabled());
  PmTestUtils::ExpectVariableNotSet(provider_->var_au_p2p_enabled());
}

TEST_F(PmRealDevicePolicyProviderTest, ValuesUpdated) {
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

  PmTestUtils::ExpectVariableHasValue(true,
                                      provider_->var_device_policy_is_loaded());

  // Test that at least one variable is set, to ensure the refresh ocurred.
  PmTestUtils::ExpectVariableHasValue(string("mychannel"),
                                      provider_->var_release_channel());
  PmTestUtils::ExpectVariableNotSet(
      provider_->var_allowed_connection_types_for_update());
}

TEST_F(PmRealDevicePolicyProviderTest, ScatterFactorConverted) {
  SetUpExistentDevicePolicy();
  EXPECT_CALL(mock_device_policy_, GetScatterFactorInSeconds(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(1234), Return(true)));
  EXPECT_TRUE(provider_->Init());

  PmTestUtils::ExpectVariableHasValue(base::TimeDelta::FromSeconds(1234),
                                      provider_->var_scatter_factor());
}

TEST_F(PmRealDevicePolicyProviderTest, NegativeScatterFactorIgnored) {
  SetUpExistentDevicePolicy();
  EXPECT_CALL(mock_device_policy_, GetScatterFactorInSeconds(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(-1), Return(true)));
  EXPECT_TRUE(provider_->Init());

  PmTestUtils::ExpectVariableNotSet(provider_->var_scatter_factor());
}

TEST_F(PmRealDevicePolicyProviderTest, AllowedTypesConverted) {
  SetUpExistentDevicePolicy();
  EXPECT_CALL(mock_device_policy_, GetAllowedConnectionTypesForUpdate(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(
                          set<string>{"bluetooth", "wifi", "not-a-type"}),
                      Return(true)));
  EXPECT_TRUE(provider_->Init());

  PmTestUtils::ExpectVariableHasValue(
      set<ConnectionType>{ConnectionType::kWifi, ConnectionType::kBluetooth},
      provider_->var_allowed_connection_types_for_update());
}

}  // namespace chromeos_policy_manager
