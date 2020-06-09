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

#include "update_engine/payload_consumer/partition_update_generator_stub.h"

#include <memory>

namespace chromeos_update_engine {

bool PartitionUpdateGeneratorStub::GenerateOperationsForPartitionsNotInPayload(
    chromeos_update_engine::BootControlInterface::Slot source_slot,
    chromeos_update_engine::BootControlInterface::Slot target_slot,
    const std::set<std::string>& partitions_in_payload,
    std::vector<PartitionUpdate>* update_list) {
  return true;
}

namespace partition_update_generator {
std::unique_ptr<PartitionUpdateGeneratorInterface> Create(
    BootControlInterface* boot_control) {
  return std::make_unique<PartitionUpdateGeneratorStub>();
}
}  // namespace partition_update_generator

}  // namespace chromeos_update_engine
