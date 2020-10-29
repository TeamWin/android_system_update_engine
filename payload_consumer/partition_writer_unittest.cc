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

#include <memory>
#include <vector>

#include <brillo/secure_blob.h>
#include <gtest/gtest.h>

#include "update_engine/common/dynamic_partition_control_stub.h"
#include "update_engine/common/error_code.h"
#include "update_engine/common/fake_prefs.h"
#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/delta_performer.h"
#include "update_engine/payload_consumer/extent_reader.h"
#include "update_engine/payload_consumer/extent_writer.h"
#include "update_engine/payload_consumer/fake_file_descriptor.h"
#include "update_engine/payload_consumer/file_descriptor.h"
#include "update_engine/payload_consumer/install_plan.h"
#include "update_engine/payload_generator/annotated_operation.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/extent_ranges.h"
#include "update_engine/payload_generator/payload_file.h"
#include "update_engine/payload_generator/payload_generation_config.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

class PartitionWriterTest : public testing::Test {
 public:
  // Helper function to pretend that the ECC file descriptor was already opened.
  // Returns a pointer to the created file descriptor.
  FakeFileDescriptor* SetFakeECCFile(size_t size) {
    EXPECT_FALSE(writer_.source_ecc_fd_) << "source_ecc_fd_ already open.";
    FakeFileDescriptor* ret = new FakeFileDescriptor();
    fake_ecc_fd_.reset(ret);
    // Call open to simulate it was already opened.
    ret->Open("", 0);
    ret->SetFileSize(size);
    writer_.source_ecc_fd_ = fake_ecc_fd_;
    return ret;
  }

  uint64_t GetSourceEccRecoveredFailures() const {
    return writer_.source_ecc_recovered_failures_;
  }

  AnnotatedOperation GenerateSourceCopyOp(const brillo::Blob& copied_data,
                                          bool add_hash,
                                          PartitionConfig* old_part = nullptr) {
    PayloadGenerationConfig config;
    const uint64_t kDefaultBlockSize = config.block_size;
    EXPECT_EQ(0U, copied_data.size() % kDefaultBlockSize);
    uint64_t num_blocks = copied_data.size() / kDefaultBlockSize;
    AnnotatedOperation aop;
    *(aop.op.add_src_extents()) = ExtentForRange(0, num_blocks);
    *(aop.op.add_dst_extents()) = ExtentForRange(0, num_blocks);
    aop.op.set_type(InstallOperation::SOURCE_COPY);
    brillo::Blob src_hash;
    EXPECT_TRUE(HashCalculator::RawHashOfData(copied_data, &src_hash));
    if (add_hash)
      aop.op.set_src_sha256_hash(src_hash.data(), src_hash.size());

    return aop;
  }

  brillo::Blob PerformSourceCopyOp(const InstallOperation& op,
                                   const brillo::Blob blob_data) {
    ScopedTempFile source_partition("Blob-XXXXXX");
    DirectExtentWriter extent_writer;
    FileDescriptorPtr fd(new EintrSafeFileDescriptor());
    EXPECT_TRUE(fd->Open(source_partition.path().c_str(), O_RDWR));
    EXPECT_TRUE(extent_writer.Init(fd, op.src_extents(), kBlockSize));
    EXPECT_TRUE(extent_writer.Write(blob_data.data(), blob_data.size()));

    ScopedTempFile target_partition("Blob-XXXXXX");

    install_part_.source_path = source_partition.path();
    install_part_.target_path = target_partition.path();
    install_part_.source_size = blob_data.size();
    install_part_.target_size = blob_data.size();

    ErrorCode error;
    EXPECT_TRUE(writer_.Init(&install_plan_, true));
    EXPECT_TRUE(writer_.PerformSourceCopyOperation(op, &error));

    brillo::Blob output_data;
    EXPECT_TRUE(utils::ReadFile(target_partition.path(), &output_data));
    return output_data;
  }

  FakePrefs prefs_{};
  InstallPlan install_plan_{};
  InstallPlan::Payload payload_{};
  DynamicPartitionControlStub dynamic_control_{};
  FileDescriptorPtr fake_ecc_fd_{};
  DeltaArchiveManifest manifest_{};
  PartitionUpdate partition_update_{};
  InstallPlan::Partition install_part_{};
  PartitionWriter writer_{
      partition_update_, install_part_, &dynamic_control_, kBlockSize, false};
};
// Test that the error-corrected file descriptor is used to read a partition
// when no hash is available for SOURCE_COPY but it falls back to the normal
// file descriptor when the size of the error corrected one is too small.
TEST_F(PartitionWriterTest, ErrorCorrectionSourceCopyWhenNoHashFallbackTest) {
  constexpr size_t kCopyOperationSize = 4 * 4096;
  ScopedTempFile source("Source-XXXXXX");
  // Setup the source path with the right expected data.
  brillo::Blob expected_data = FakeFileDescriptorData(kCopyOperationSize);
  EXPECT_TRUE(test_utils::WriteFileVector(source.path(), expected_data));

  // Setup the fec file descriptor as the fake stream, with smaller data than
  // the expected.
  FakeFileDescriptor* fake_fec = SetFakeECCFile(kCopyOperationSize / 2);

  PartitionConfig old_part(kPartitionNameRoot);
  old_part.path = source.path();
  old_part.size = expected_data.size();

  // The payload operation doesn't include an operation hash.
  auto source_copy_op = GenerateSourceCopyOp(expected_data, false, &old_part);

  auto output_data = PerformSourceCopyOp(source_copy_op.op, expected_data);
  ASSERT_EQ(output_data, expected_data);

  // Verify that the fake_fec was attempted to be used. Since the file
  // descriptor is shorter it can actually do more than one read to realize it
  // reached the EOF.
  EXPECT_LE(1U, fake_fec->GetReadOps().size());
  // This fallback doesn't count as an error-corrected operation since the
  // operation hash was not available.
  EXPECT_EQ(0U, GetSourceEccRecoveredFailures());
}

// Test that the error-corrected file descriptor is used to read the partition
// since the source partition doesn't match the operation hash.
TEST_F(PartitionWriterTest, ErrorCorrectionSourceCopyFallbackTest) {
  constexpr size_t kCopyOperationSize = 4 * 4096;
  // Write invalid data to the source image, which doesn't match the expected
  // hash.
  brillo::Blob invalid_data(kCopyOperationSize, 0x55);

  // Setup the fec file descriptor as the fake stream, which matches
  // |expected_data|.
  FakeFileDescriptor* fake_fec = SetFakeECCFile(kCopyOperationSize);
  brillo::Blob expected_data = FakeFileDescriptorData(kCopyOperationSize);

  auto source_copy_op = GenerateSourceCopyOp(expected_data, true);
  auto output_data = PerformSourceCopyOp(source_copy_op.op, invalid_data);
  ASSERT_EQ(output_data, expected_data);

  // Verify that the fake_fec was actually used.
  EXPECT_EQ(1U, fake_fec->GetReadOps().size());
  EXPECT_EQ(1U, GetSourceEccRecoveredFailures());
}

TEST_F(PartitionWriterTest, ChooseSourceFDTest) {
  constexpr size_t kSourceSize = 4 * 4096;
  ScopedTempFile source("Source-XXXXXX");
  // Write invalid data to the source image, which doesn't match the expected
  // hash.
  brillo::Blob invalid_data(kSourceSize, 0x55);
  EXPECT_TRUE(test_utils::WriteFileVector(source.path(), invalid_data));

  writer_.source_fd_ = std::make_shared<EintrSafeFileDescriptor>();
  writer_.source_fd_->Open(source.path().c_str(), O_RDONLY);

  // Setup the fec file descriptor as the fake stream, which matches
  // |expected_data|.
  FakeFileDescriptor* fake_fec = SetFakeECCFile(kSourceSize);
  brillo::Blob expected_data = FakeFileDescriptorData(kSourceSize);

  InstallOperation op;
  *(op.add_src_extents()) = ExtentForRange(0, kSourceSize / 4096);
  brillo::Blob src_hash;
  EXPECT_TRUE(HashCalculator::RawHashOfData(expected_data, &src_hash));
  op.set_src_sha256_hash(src_hash.data(), src_hash.size());

  ErrorCode error = ErrorCode::kSuccess;
  EXPECT_EQ(writer_.source_ecc_fd_, writer_.ChooseSourceFD(op, &error));
  EXPECT_EQ(ErrorCode::kSuccess, error);
  // Verify that the fake_fec was actually used.
  EXPECT_EQ(1U, fake_fec->GetReadOps().size());
  EXPECT_EQ(1U, GetSourceEccRecoveredFailures());
}

}  // namespace chromeos_update_engine
