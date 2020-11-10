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

#include <memory>

#include "update_engine/update_manager/minimum_version_policy_impl.h"
#include "update_engine/update_manager/policy_test_utils.h"

using chromeos_update_engine::ErrorCode;
using chromeos_update_engine::InstallPlan;

namespace {

const char* kInvalidVersion = "13315.woops.12";
const char* kOldVersion = "13315.60.12";
const char* kNewVersion = "13315.60.15";

}  // namespace

namespace chromeos_update_manager {

class UmMinimumVersionPolicyImplTest : public UmPolicyTestBase {
 protected:
  UmMinimumVersionPolicyImplTest() {
    policy_ = std::make_unique<MinimumVersionPolicyImpl>();
  }

  void SetCurrentVersion(const std::string& version) {
    fake_state_.system_provider()->var_chromeos_version()->reset(
        new base::Version(version));
  }

  void SetMinimumVersion(const std::string& version) {
    fake_state_.device_policy_provider()->var_device_minimum_version()->reset(
        new base::Version(version));
  }

  void TestPolicy(const EvalStatus& expected_status) {
    InstallPlan install_plan;
    ErrorCode result;
    ExpectPolicyStatus(
        expected_status, &Policy::UpdateCanBeApplied, &result, &install_plan);
    if (expected_status == EvalStatus::kSucceeded)
      EXPECT_EQ(result, ErrorCode::kSuccess);
  }
};

TEST_F(UmMinimumVersionPolicyImplTest, ContinueWhenCurrentVersionIsNotSet) {
  SetMinimumVersion(kNewVersion);

  TestPolicy(EvalStatus::kContinue);
}

TEST_F(UmMinimumVersionPolicyImplTest, ContinueWhenCurrentVersionIsInvalid) {
  SetCurrentVersion(kInvalidVersion);
  SetMinimumVersion(kNewVersion);

  TestPolicy(EvalStatus::kContinue);
}

TEST_F(UmMinimumVersionPolicyImplTest, ContinueWhenMinumumVersionIsNotSet) {
  SetCurrentVersion(kOldVersion);

  TestPolicy(EvalStatus::kContinue);
}

TEST_F(UmMinimumVersionPolicyImplTest, ContinueWhenMinumumVersionIsInvalid) {
  SetCurrentVersion(kOldVersion);
  SetMinimumVersion(kInvalidVersion);

  TestPolicy(EvalStatus::kContinue);
}

TEST_F(UmMinimumVersionPolicyImplTest,
       ContinueWhenCurrentVersionIsGreaterThanMinimumVersion) {
  SetCurrentVersion(kNewVersion);
  SetMinimumVersion(kOldVersion);

  TestPolicy(EvalStatus::kContinue);
}

TEST_F(UmMinimumVersionPolicyImplTest,
       ContinueWhenCurrentVersionIsEqualToMinimumVersion) {
  SetCurrentVersion(kNewVersion);
  SetMinimumVersion(kNewVersion);

  TestPolicy(EvalStatus::kContinue);
}

TEST_F(UmMinimumVersionPolicyImplTest,
       SuccessWhenCurrentVersionIsLessThanMinimumVersion) {
  SetCurrentVersion(kOldVersion);
  SetMinimumVersion(kNewVersion);

  TestPolicy(EvalStatus::kSucceeded);
}

}  // namespace chromeos_update_manager
