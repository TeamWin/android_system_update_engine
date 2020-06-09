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

#ifndef UPDATE_ENGINE_PAYLOAD_CONSUMER_PARTITION_UPDATE_GENERATOR_STUB_H_
#define UPDATE_ENGINE_PAYLOAD_CONSUMER_PARTITION_UPDATE_GENERATOR_STUB_H_

#include <set>
#include <string>
#include <vector>

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/payload_consumer/partition_update_generator_interface.h"

namespace chromeos_update_engine {
class PartitionUpdateGeneratorStub : public PartitionUpdateGeneratorInterface {
 public:
  PartitionUpdateGeneratorStub() = default;
  bool GenerateOperationsForPartitionsNotInPayload(
      BootControlInterface::Slot source_slot,
      BootControlInterface::Slot target_slot,
      const std::set<std::string>& partitions_in_payload,
      std::vector<PartitionUpdate>* update_list) override;
};

}  // namespace chromeos_update_engine

#endif
