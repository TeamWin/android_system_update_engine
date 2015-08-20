//
// Copyright (C) 2010 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_FULL_UPDATE_GENERATOR_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_FULL_UPDATE_GENERATOR_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "update_engine/payload_generator/blob_file_writer.h"
#include "update_engine/payload_generator/operations_generator.h"
#include "update_engine/payload_generator/payload_generation_config.h"

namespace chromeos_update_engine {

class FullUpdateGenerator : public OperationsGenerator {
 public:
  FullUpdateGenerator() = default;

  // OperationsGenerator override.
  // Creates a full update for the target image defined in |config|. |config|
  // must be a valid payload generation configuration for a full payload.
  // Populates |rootfs_ops| and |kernel_ops|, with data about the update
  // operations, and writes relevant data to |data_file_fd|, updating
  // |data_file_size| as it does.
  bool GenerateOperations(
      const PayloadGenerationConfig& config,
      BlobFileWriter* blob_file,
      std::vector<AnnotatedOperation>* rootfs_ops,
      std::vector<AnnotatedOperation>* kernel_ops) override;

  // Generates the list of operations to update inplace from the partition
  // |old_part| to |new_part|. The |partition_size| should be at least
  // |new_part.size| and any extra space there could be used as scratch space.
  // The operations generated will not write more than |chunk_blocks| blocks.
  // The new operations will create blobs in |data_file_fd| and update
  // the file size pointed by |data_file_size| if needed.
  // On success, stores the new operations in |aops| and returns true.
  static bool GenerateOperationsForPartition(
      const PartitionConfig& new_part,
      size_t block_size,
      size_t chunk_blocks,
      BlobFileWriter* blob_file,
      std::vector<AnnotatedOperation>* aops);

 private:
  DISALLOW_COPY_AND_ASSIGN(FullUpdateGenerator);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_FULL_UPDATE_GENERATOR_H_
