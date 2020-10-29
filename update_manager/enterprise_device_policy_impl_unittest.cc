//
// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/update_manager/enterprise_device_policy_impl.h"

#include <memory>

#include "update_engine/update_manager/policy_test_utils.h"

namespace chromeos_update_manager {

class UmEnterpriseDevicePolicyImplTest : public UmPolicyTestBase {
 protected:
  UmEnterpriseDevicePolicyImplTest() : UmPolicyTestBase() {
    policy_ = std::make_unique<EnterpriseDevicePolicyImpl>();
  }

  void SetUpDefaultState() override {
    UmPolicyTestBase::SetUpDefaultState();

    fake_state_.device_policy_provider()->var_device_policy_is_loaded()->reset(
        new bool(true));
  }
};

TEST_F(UmEnterpriseDevicePolicyImplTest, KioskAppVersionSet) {
  fake_state_.device_policy_provider()->var_update_disabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()
      ->var_allow_kiosk_app_control_chrome_version()
      ->reset(new bool(true));

  fake_state_.system_provider()->var_kiosk_required_platform_version()->reset(
      new std::string("1234.5.6"));

  UpdateCheckParams result;
  ExpectPolicyStatus(
      EvalStatus::kContinue, &Policy::UpdateCheckAllowed, &result);
  EXPECT_EQ(result.target_version_prefix, "1234.5.6");
}

TEST_F(UmEnterpriseDevicePolicyImplTest, KioskAppVersionUnreadableNoUpdate) {
  fake_state_.device_policy_provider()->var_update_disabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()
      ->var_allow_kiosk_app_control_chrome_version()
      ->reset(new bool(true));

  fake_state_.system_provider()->var_kiosk_required_platform_version()->reset(
      nullptr);

  UpdateCheckParams result;
  ExpectPolicyStatus(
      EvalStatus::kAskMeAgainLater, &Policy::UpdateCheckAllowed, &result);
}

TEST_F(UmEnterpriseDevicePolicyImplTest, KioskAppVersionUnreadableUpdate) {
  fake_state_.device_policy_provider()->var_update_disabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()
      ->var_allow_kiosk_app_control_chrome_version()
      ->reset(new bool(true));

  // The real variable returns an empty string after several unsuccessful
  // reading attempts. Fake this by setting it directly to empty string.
  fake_state_.system_provider()->var_kiosk_required_platform_version()->reset(
      new std::string(""));

  UpdateCheckParams result;
  ExpectPolicyStatus(
      EvalStatus::kContinue, &Policy::UpdateCheckAllowed, &result);
  EXPECT_EQ(result.target_version_prefix, "");
}

TEST_F(UmEnterpriseDevicePolicyImplTest,
       KioskAppVersionUnreadableUpdateWithMinVersion) {
  fake_state_.device_policy_provider()->var_update_disabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()
      ->var_allow_kiosk_app_control_chrome_version()
      ->reset(new bool(true));

  // The real variable returns an empty string after several unsuccessful
  // reading attempts. Fake this by setting it directly to empty string.
  fake_state_.system_provider()->var_kiosk_required_platform_version()->reset(
      new std::string(""));
  // Update if the minimum version is above the current OS version.
  fake_state_.device_policy_provider()->var_device_minimum_version()->reset(
      new base::Version("2.0.0"));
  fake_state_.system_provider()->var_chromeos_version()->reset(
      new base::Version("1.0.0"));

  UpdateCheckParams result;
  ExpectPolicyStatus(
      EvalStatus::kContinue, &Policy::UpdateCheckAllowed, &result);
  EXPECT_EQ(result.target_version_prefix, "");
}

TEST_F(UmEnterpriseDevicePolicyImplTest,
       KioskAppVersionUnreadableNoUpdateWithMinVersion) {
  fake_state_.device_policy_provider()->var_update_disabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()
      ->var_allow_kiosk_app_control_chrome_version()
      ->reset(new bool(true));

  // The real variable returns an empty string after several unsuccessful
  // reading attempts. Fake this by setting it directly to empty string.
  fake_state_.system_provider()->var_kiosk_required_platform_version()->reset(
      new std::string(""));
  // Block update if the minimum version is below the current OS version.
  fake_state_.device_policy_provider()->var_device_minimum_version()->reset(
      new base::Version("1.0.0"));
  fake_state_.system_provider()->var_chromeos_version()->reset(
      new base::Version("2.0.0"));

  UpdateCheckParams result;
  ExpectPolicyStatus(
      EvalStatus::kAskMeAgainLater, &Policy::UpdateCheckAllowed, &result);
}

TEST_F(UmEnterpriseDevicePolicyImplTest, ChannelDowngradeBehaviorNoRollback) {
  fake_state_.device_policy_provider()->var_release_channel_delegated()->reset(
      new bool(false));
  fake_state_.device_policy_provider()->var_release_channel()->reset(
      new std::string("stable-channel"));

  UpdateCheckParams result;
  ExpectPolicyStatus(
      EvalStatus::kContinue, &Policy::UpdateCheckAllowed, &result);
  EXPECT_FALSE(result.rollback_on_channel_downgrade);
}

TEST_F(UmEnterpriseDevicePolicyImplTest, ChannelDowngradeBehaviorRollback) {
  fake_state_.device_policy_provider()->var_release_channel_delegated()->reset(
      new bool(false));
  fake_state_.device_policy_provider()->var_release_channel()->reset(
      new std::string("stable-channel"));
  fake_state_.device_policy_provider()->var_channel_downgrade_behavior()->reset(
      new ChannelDowngradeBehavior(ChannelDowngradeBehavior::kRollback));

  UpdateCheckParams result;
  ExpectPolicyStatus(
      EvalStatus::kContinue, &Policy::UpdateCheckAllowed, &result);
  EXPECT_TRUE(result.rollback_on_channel_downgrade);
}

}  // namespace chromeos_update_manager
