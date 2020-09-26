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

#ifndef UPDATE_ENGINE_PAYLOAD_CONSUMER_PARTITION_UPDATE_GENERATOR_INTERFACE_H_
#define UPDATE_ENGINE_PAYLOAD_CONSUMER_PARTITION_UPDATE_GENERATOR_INTERFACE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "update_engine/common/boot_control_interface.h"

namespace chromeos_update_engine {
class PartitionUpdate;

// This class parses the partitions that are not included in the payload of a
// partial A/B update. And it generates additional operations for these
// partitions to make the update complete.
class PartitionUpdateGeneratorInterface {
 public:
  virtual ~PartitionUpdateGeneratorInterface() = default;

  // Adds PartitionUpdate for partitions not included in the payload. For static
  // partitions, it generates SOURCE_COPY operations to copy the bytes from the
  // source slot to target slot. For dynamic partitions, it only calculates the
  // partition hash for the filesystem verification later.
  virtual bool GenerateOperationsForPartitionsNotInPayload(
      BootControlInterface::Slot source_slot,
      BootControlInterface::Slot target_slot,
      const std::set<std::string>& partitions_in_payload,
      std::vector<PartitionUpdate>* update_list) = 0;
};

namespace partition_update_generator {
std::unique_ptr<PartitionUpdateGeneratorInterface> Create(
    BootControlInterface* boot_control, size_t block_size);
}

}  // namespace chromeos_update_engine

#endif
