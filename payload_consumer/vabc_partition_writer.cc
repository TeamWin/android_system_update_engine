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

#include "update_engine/payload_consumer/vabc_partition_writer.h"

#include <memory>
#include <vector>

#include <libsnapshot/cow_writer.h>

#include "update_engine/common/cow_operation_convert.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/extent_writer.h"
#include "update_engine/payload_consumer/install_plan.h"
#include "update_engine/payload_consumer/partition_writer.h"
#include "update_engine/payload_consumer/snapshot_extent_writer.h"

namespace chromeos_update_engine {
bool VABCPartitionWriter::Init(const InstallPlan* install_plan,
                               bool source_may_exist) {
  TEST_AND_RETURN_FALSE(install_plan != nullptr);
  TEST_AND_RETURN_FALSE(PartitionWriter::Init(install_plan, source_may_exist));
  cow_writer_ = dynamic_control_->OpenCowWriter(
      install_part_.name, install_part_.source_path, install_plan->is_resume);
  TEST_AND_RETURN_FALSE(cow_writer_ != nullptr);

  // TODO(zhangkelvin) Emit a label before writing SOURCE_COPY. When resuming,
  // use pref or CowWriter::GetLastLabel to determine if the SOURCE_COPY ops are
  // written. No need to handle SOURCE_COPY operations when resuming.

  // ===== Resume case handling code goes here ====

  // ==============================================

  // TODO(zhangkelvin) Rewrite this in C++20 coroutine once that's available.
  auto converted = ConvertToCowOperations(partition_update_.operations(),
                                          partition_update_.merge_operations());
  std::vector<uint8_t> buffer(block_size_);
  for (const auto& cow_op : converted) {
    switch (cow_op.op) {
      case CowOperation::CowCopy:
        TEST_AND_RETURN_FALSE(
            cow_writer_->AddCopy(cow_op.dst_block, cow_op.src_block));
        break;
      case CowOperation::CowReplace:
        ssize_t bytes_read = 0;
        TEST_AND_RETURN_FALSE(utils::PReadAll(source_fd_,
                                              buffer.data(),
                                              block_size_,
                                              cow_op.src_block * block_size_,
                                              &bytes_read));
        if (bytes_read <= 0 || static_cast<size_t>(bytes_read) != block_size_) {
          LOG(ERROR) << "source_fd->Read failed: " << bytes_read;
          return false;
        }
        TEST_AND_RETURN_FALSE(cow_writer_->AddRawBlocks(
            cow_op.dst_block, buffer.data(), block_size_));
        break;
    }
  }
  return true;
}

std::unique_ptr<ExtentWriter> VABCPartitionWriter::CreateBaseExtentWriter() {
  return std::make_unique<SnapshotExtentWriter>(cow_writer_.get());
}

[[nodiscard]] bool VABCPartitionWriter::PerformZeroOrDiscardOperation(
    const InstallOperation& operation) {
  for (const auto& extent : operation.dst_extents()) {
    TEST_AND_RETURN_FALSE(
        cow_writer_->AddZeroBlocks(extent.start_block(), extent.num_blocks()));
  }
  return true;
}

[[nodiscard]] bool VABCPartitionWriter::PerformSourceCopyOperation(
    const InstallOperation& operation, ErrorCode* error) {
  // TODO(zhangkelvin) Probably just ignore SOURCE_COPY? They should be taken
  // care of during Init();
  return true;
}

bool VABCPartitionWriter::Flush() {
  // No need to do anything, as CowWriter automatically flushes every OP added.
  return true;
}

VABCPartitionWriter::~VABCPartitionWriter() {
  cow_writer_->Finalize();
}

}  // namespace chromeos_update_engine
