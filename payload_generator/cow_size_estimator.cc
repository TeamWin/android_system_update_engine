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

#include "update_engine/payload_generator/cow_size_estimator.h"

#include <utility>
#include <vector>

#include <libsnapshot/cow_writer.h>

#include "android-base/unique_fd.h"
#include "update_engine/common/cow_operation_convert.h"
#include "update_engine/payload_consumer/vabc_partition_writer.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {
using android::snapshot::CowWriter;

void PerformReplaceOp(const InstallOperation& op,
                      CowWriter* writer,
                      FileDescriptorPtr target_fd,
                      size_t block_size) {
  std::vector<unsigned char> buffer;
  for (const auto& extent : op.dst_extents()) {
    buffer.resize(extent.num_blocks() * block_size);
    // No need to read from payload.bin then decompress, just read from target
    // directly.
    ssize_t bytes_read = 0;
    auto success = utils::ReadAll(target_fd,
                                  buffer.data(),
                                  buffer.size(),
                                  extent.start_block() * block_size,
                                  &bytes_read);
    CHECK(success);
    CHECK_EQ(static_cast<size_t>(bytes_read), buffer.size());
    writer->AddRawBlocks(extent.start_block(), buffer.data(), buffer.size());
  }
}

void PerformZeroOp(const InstallOperation& op,
                   CowWriter* writer,
                   size_t block_size) {
  for (const auto& extent : op.dst_extents()) {
    writer->AddZeroBlocks(extent.start_block(), extent.num_blocks());
  }
}

size_t EstimateCowSize(
    FileDescriptorPtr source_fd,
    FileDescriptorPtr target_fd,
    const google::protobuf::RepeatedPtrField<InstallOperation>& operations,
    const google::protobuf::RepeatedPtrField<CowMergeOperation>&
        merge_operations,
    size_t block_size) {
  android::snapshot::CowWriter cow_writer{
      {.block_size = static_cast<uint32_t>(block_size), .compression = "gz"}};
  // CowWriter treats -1 as special value, will discard all the data but still
  // reports Cow size. Good for estimation purposes
  cow_writer.Initialize(android::base::borrowed_fd{-1});

  const auto converted = ConvertToCowOperations(operations, merge_operations);
  VABCPartitionWriter::WriteAllCowOps(
      block_size, converted, &cow_writer, source_fd);
  cow_writer.AddLabel(0);
  for (const auto& op : operations) {
    switch (op.type()) {
      case InstallOperation::REPLACE:
      case InstallOperation::REPLACE_BZ:
      case InstallOperation::REPLACE_XZ:
        PerformReplaceOp(op, &cow_writer, target_fd, block_size);
        break;
      case InstallOperation::ZERO:
      case InstallOperation::DISCARD:
        PerformZeroOp(op, &cow_writer, block_size);
        break;
      case InstallOperation::SOURCE_COPY:
      case InstallOperation::MOVE:
        // Already handeled by WriteAllCowOps,
        break;
      case InstallOperation::SOURCE_BSDIFF:
      case InstallOperation::BROTLI_BSDIFF:
      case InstallOperation::PUFFDIFF:
      case InstallOperation::BSDIFF:
        // We might do something special by adding CowBsdiff to CowWriter.
        // For now proceed the same way as normal REPLACE operation.
        PerformReplaceOp(op, &cow_writer, target_fd, block_size);
        break;
    }
    // Arbitrary label number, we won't be resuming use these labels here.
    // They are emitted just to keep size estimates accurate. As update_engine
    // emits 1 label for every op.
    cow_writer.AddLabel(2);
  }
  // TODO(zhangkelvin) Take FEC extents into account once VABC stabilizes
  return cow_writer.GetCowSize();
}
}  // namespace chromeos_update_engine
