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

#ifndef UPDATE_ENGINE_VABC_PARTITION_WRITER_H_
#define UPDATE_ENGINE_VABC_PARTITION_WRITER_H_

#include <memory>
#include <vector>

#include <libsnapshot/snapshot_writer.h>

#include "update_engine/common/cow_operation_convert.h"
#include "update_engine/payload_consumer/install_plan.h"
#include "update_engine/payload_consumer/partition_writer.h"

namespace chromeos_update_engine {
class VABCPartitionWriter final : public PartitionWriter {
 public:
  using PartitionWriter::PartitionWriter;
  [[nodiscard]] bool Init(const InstallPlan* install_plan,
                          bool source_may_exist) override;
  ~VABCPartitionWriter() override;

  [[nodiscard]] std::unique_ptr<ExtentWriter> CreateBaseExtentWriter() override;

  // Only ZERO and SOURCE_COPY InstallOperations are treated special by VABC
  // Partition Writer. These operations correspond to COW_ZERO and COW_COPY. All
  // other operations just get converted to COW_REPLACE.
  [[nodiscard]] bool PerformZeroOrDiscardOperation(
      const InstallOperation& operation) override;
  [[nodiscard]] bool PerformSourceCopyOperation(
      const InstallOperation& operation, ErrorCode* error) override;
  [[nodiscard]] bool Flush() override;

  static bool WriteAllCowOps(size_t block_size,
                             const std::vector<CowOperation>& converted,
                             android::snapshot::ICowWriter* cow_writer,
                             FileDescriptorPtr source_fd);

 private:
  std::unique_ptr<android::snapshot::ISnapshotWriter> cow_writer_;
};

}  // namespace chromeos_update_engine

#endif
