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

#include "update_engine/update_manager/enterprise_rollback_policy_impl.h"

using chromeos_update_engine::ErrorCode;
using chromeos_update_engine::InstallPlan;
using std::string;

namespace chromeos_update_manager {

EvalStatus EnterpriseRollbackPolicyImpl::UpdateCanBeApplied(
    EvaluationContext* ec,
    State* state,
    string* error,
    ErrorCode* result,
    InstallPlan* install_plan) const {
  if (install_plan && install_plan->is_rollback) {
    LOG(INFO)
        << "Update is enterprise rollback, allowing update to be applied.";
    *result = ErrorCode::kSuccess;
    return EvalStatus::kSucceeded;
  }
  return EvalStatus::kContinue;
}

}  // namespace chromeos_update_manager
