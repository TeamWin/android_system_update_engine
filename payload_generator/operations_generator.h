//
// Copyright (C) 2015 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_OPERATIONS_GENERATOR_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_OPERATIONS_GENERATOR_H_

#include <vector>

#include <base/macros.h>

#include "update_engine/payload_generator/annotated_operation.h"
#include "update_engine/payload_generator/blob_file_writer.h"
#include "update_engine/payload_generator/payload_generation_config.h"

namespace chromeos_update_engine {

class OperationsGenerator {
 public:
  virtual ~OperationsGenerator() = default;

  // This method generates two lists of operations, one for the rootfs and one
  // for the kernel and stores the generated operations in |rootfs_ops| and
  // |kernel_ops| respectivelly. These operations are generated based on the
  // given |config|. The operations should be applied in the order specified in
  // the list, and they respect the payload version and type (delta or full)
  // specified in |config|.
  // The operations generated will refer to offsets in the file |data_file_fd|,
  // where this function stores the output, but not necessarily in the same
  // order as they appear in the |rootfs_ops| and |kernel_ops|.
  // This function stores the amount of data written to |data_file_fd| in
  // |data_file_size|.
  virtual bool GenerateOperations(
      const PayloadGenerationConfig& config,
      BlobFileWriter* blob_file,
      std::vector<AnnotatedOperation>* rootfs_ops,
      std::vector<AnnotatedOperation>* kernel_ops) = 0;

 protected:
  OperationsGenerator() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(OperationsGenerator);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_OPERATIONS_GENERATOR_H_
