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

#include "update_engine/update_manager/enterprise_rollback_policy_impl.h"
#include "update_engine/update_manager/policy_test_utils.h"
#include "update_engine/update_manager/weekly_time.h"

using chromeos_update_engine::ErrorCode;
using chromeos_update_engine::InstallPlan;

namespace chromeos_update_manager {

class UmEnterpriseRollbackPolicyImplTest : public UmPolicyTestBase {
 protected:
  UmEnterpriseRollbackPolicyImplTest() {
    policy_ = std::make_unique<EnterpriseRollbackPolicyImpl>();
  }
};

TEST_F(UmEnterpriseRollbackPolicyImplTest,
       ContinueWhenUpdateIsNotEnterpriseRollback) {
  InstallPlan install_plan{.is_rollback = false};
  ErrorCode result;
  ExpectPolicyStatus(EvalStatus::kContinue,
                     &Policy::UpdateCanBeApplied,
                     &result,
                     &install_plan);
}

TEST_F(UmEnterpriseRollbackPolicyImplTest,
       SuccessWhenUpdateIsEnterpriseRollback) {
  InstallPlan install_plan{.is_rollback = true};
  ErrorCode result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateCanBeApplied,
                     &result,
                     &install_plan);
  EXPECT_EQ(result, ErrorCode::kSuccess);
}

}  // namespace chromeos_update_manager
