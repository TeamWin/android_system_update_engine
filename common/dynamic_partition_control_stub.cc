//
// Copyright (C) 2019 The Android Open Source Project
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

#include <stdint.h>

#include <string>

#include <base/logging.h>

#include "update_engine/common/dynamic_partition_control_stub.h"

namespace chromeos_update_engine {

FeatureFlag DynamicPartitionControlStub::GetDynamicPartitionsFeatureFlag() {
  return FeatureFlag(FeatureFlag::Value::NONE);
}

FeatureFlag DynamicPartitionControlStub::GetVirtualAbFeatureFlag() {
  return FeatureFlag(FeatureFlag::Value::NONE);
}

void DynamicPartitionControlStub::Cleanup() {}

bool DynamicPartitionControlStub::PreparePartitionsForUpdate(
    uint32_t source_slot,
    uint32_t target_slot,
    const DeltaArchiveManifest& manifest,
    bool update) {
  return true;
}

bool DynamicPartitionControlStub::FinishUpdate() {
  return true;
}

}  // namespace chromeos_update_engine
