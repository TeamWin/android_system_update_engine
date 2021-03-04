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

#include <string>
#include <utility>
#include <vector>

#include <android-base/unique_fd.h>
#include <libsnapshot/cow_writer.h>

#include "update_engine/common/cow_operation_convert.h"
#include "update_engine/common/utils.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {
using android::snapshot::CowWriter;

namespace {
bool PerformReplaceOp(const InstallOperation& op,
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
    TEST_AND_RETURN_FALSE(success);
    CHECK_EQ(static_cast<size_t>(bytes_read), buffer.size());
    TEST_AND_RETURN_FALSE(writer->AddRawBlocks(
        extent.start_block(), buffer.data(), buffer.size()));
  }
  return true;
}

bool PerformZeroOp(const InstallOperation& op,
                   CowWriter* writer,
                   size_t block_size) {
  for (const auto& extent : op.dst_extents()) {
    TEST_AND_RETURN_FALSE(
        writer->AddZeroBlocks(extent.start_block(), extent.num_blocks()));
  }
  return true;
}

bool WriteAllCowOps(size_t block_size,
                    const std::vector<CowOperation>& converted,
                    android::snapshot::ICowWriter* cow_writer,
                    FileDescriptorPtr target_fd) {
  std::vector<uint8_t> buffer(block_size);

  for (const auto& cow_op : converted) {
    switch (cow_op.op) {
      case CowOperation::CowCopy:
        if (cow_op.src_block == cow_op.dst_block) {
          continue;
        }
        TEST_AND_RETURN_FALSE(
            cow_writer->AddCopy(cow_op.dst_block, cow_op.src_block));
        break;
      case CowOperation::CowReplace:
        ssize_t bytes_read = 0;
        TEST_AND_RETURN_FALSE(chromeos_update_engine::utils::ReadAll(
            target_fd,
            buffer.data(),
            block_size,
            cow_op.dst_block * block_size,
            &bytes_read));
        if (bytes_read <= 0 || static_cast<size_t>(bytes_read) != block_size) {
          LOG(ERROR) << "source_fd->Read failed: " << bytes_read;
          return false;
        }
        TEST_AND_RETURN_FALSE(cow_writer->AddRawBlocks(
            cow_op.dst_block, buffer.data(), block_size));
        break;
    }
  }

  return true;
}
}  // namespace

size_t EstimateCowSize(
    FileDescriptorPtr target_fd,
    const google::protobuf::RepeatedPtrField<InstallOperation>& operations,
    const google::protobuf::RepeatedPtrField<CowMergeOperation>&
        merge_operations,
    size_t block_size,
    std::string compression) {
  android::snapshot::CowWriter cow_writer{
      {.block_size = static_cast<uint32_t>(block_size),
       .compression = std::move(compression)}};
  // CowWriter treats -1 as special value, will discard all the data but still
  // reports Cow size. Good for estimation purposes
  cow_writer.Initialize(android::base::borrowed_fd{-1});
  CHECK(CowDryRun(
      target_fd, operations, merge_operations, block_size, &cow_writer));
  CHECK(cow_writer.Finalize());
  return cow_writer.GetCowSize();
}

bool CowDryRun(
    FileDescriptorPtr target_fd,
    const google::protobuf::RepeatedPtrField<InstallOperation>& operations,
    const google::protobuf::RepeatedPtrField<CowMergeOperation>&
        merge_operations,
    size_t block_size,
    android::snapshot::CowWriter* cow_writer) {
  const auto converted = ConvertToCowOperations(operations, merge_operations);
  WriteAllCowOps(block_size, converted, cow_writer, target_fd);
  cow_writer->AddLabel(0);
  for (const auto& op : operations) {
    switch (op.type()) {
      case InstallOperation::REPLACE:
      case InstallOperation::REPLACE_BZ:
      case InstallOperation::REPLACE_XZ:
        TEST_AND_RETURN_FALSE(
            PerformReplaceOp(op, cow_writer, target_fd, block_size));
        break;
      case InstallOperation::ZERO:
      case InstallOperation::DISCARD:
        TEST_AND_RETURN_FALSE(PerformZeroOp(op, cow_writer, block_size));
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
        TEST_AND_RETURN_FALSE(
            PerformReplaceOp(op, cow_writer, target_fd, block_size));
        break;
    }
    // Arbitrary label number, we won't be resuming use these labels here.
    // They are emitted just to keep size estimates accurate. As update_engine
    // emits 1 label for every op.
    cow_writer->AddLabel(2);
  }
  // TODO(zhangkelvin) Take FEC extents into account once VABC stabilizes
  return true;
}
}  // namespace chromeos_update_engine
