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

#include <cstddef>
#include <memory>

#include <base/logging.h>

#include "update_engine/payload_consumer/partition_writer.h"

namespace chromeos_update_engine::partition_writer {
std::unique_ptr<PartitionWriter> CreatePartitionWriter(
    const PartitionUpdate& partition_update,
    const InstallPlan::Partition& install_part,
    DynamicPartitionControlInterface* dynamic_control,
    size_t block_size,
    PrefsInterface* prefs,
    bool is_interactive,
    bool is_dynamic_partition) {
  return std::make_unique<PartitionWriter>(partition_update,
                                           install_part,
                                           dynamic_control,
                                           block_size,
                                           prefs,
                                           is_interactive);
}
}  // namespace chromeos_update_engine::partition_writer
