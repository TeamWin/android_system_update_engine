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

#include "update_engine/update_manager/minimum_version_policy_impl.h"

#include <base/version.h>

using chromeos_update_engine::ErrorCode;
using chromeos_update_engine::InstallPlan;

namespace chromeos_update_manager {

EvalStatus MinimumVersionPolicyImpl::UpdateCanBeApplied(
    EvaluationContext* ec,
    State* state,
    std::string* error,
    ErrorCode* result,
    InstallPlan* install_plan) const {
  const base::Version* current_version(
      ec->GetValue(state->system_provider()->var_chromeos_version()));
  if (current_version == nullptr || !current_version->IsValid()) {
    LOG(WARNING) << "Unable to access current version";
    return EvalStatus::kContinue;
  }

  const base::Version* minimum_version = ec->GetValue(
      state->device_policy_provider()->var_device_minimum_version());
  if (minimum_version == nullptr || !minimum_version->IsValid()) {
    LOG(WARNING) << "Unable to access minimum version";
    return EvalStatus::kContinue;
  }

  if (*current_version < *minimum_version) {
    LOG(INFO) << "Updating from version less than minimum required"
                 ", allowing update to be applied.";
    *result = ErrorCode::kSuccess;
    return EvalStatus::kSucceeded;
  }

  return EvalStatus::kContinue;
}

}  // namespace chromeos_update_manager
