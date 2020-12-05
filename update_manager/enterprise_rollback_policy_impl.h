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

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_ENTERPRISE_ROLLBACK_POLICY_IMPL_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_ENTERPRISE_ROLLBACK_POLICY_IMPL_H_

#include <string>

#include "update_engine/common/error_code.h"
#include "update_engine/payload_consumer/install_plan.h"
#include "update_engine/update_manager/policy_utils.h"

namespace chromeos_update_manager {

// If the update is an enterprise rollback, this should not block the update
// to be applied.
class EnterpriseRollbackPolicyImpl : public PolicyImplBase {
 public:
  EnterpriseRollbackPolicyImpl() = default;
  ~EnterpriseRollbackPolicyImpl() override = default;

  // Policy overrides.
  EvalStatus UpdateCanBeApplied(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      chromeos_update_engine::ErrorCode* result,
      chromeos_update_engine::InstallPlan* install_plan) const override;

 protected:
  std::string PolicyName() const override {
    return "EnterpriseRollbackPolicyImpl";
  }

 private:
  EnterpriseRollbackPolicyImpl(const EnterpriseRollbackPolicyImpl&) = delete;
  EnterpriseRollbackPolicyImpl& operator=(const EnterpriseRollbackPolicyImpl&) =
      delete;
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_ENTERPRISE_ROLLBACK_POLICY_IMPL_H_
