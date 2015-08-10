// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/ab_generator.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "update_engine/bzip.h"
#include "update_engine/payload_generator/annotated_operation.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/extent_ranges.h"
#include "update_engine/payload_generator/extent_utils.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

bool ExtentEquals(Extent ext, uint64_t start_block, uint64_t num_blocks) {
  return ext.start_block() == start_block && ext.num_blocks() == num_blocks;
}

// Tests splitting of a REPLACE/REPLACE_BZ operation.
void TestSplitReplaceOrReplaceBzOperation(
    DeltaArchiveManifest_InstallOperation_Type orig_type,
    bool compressible) {
  const size_t op_ex1_start_block = 2;
  const size_t op_ex1_num_blocks = 2;
  const size_t op_ex2_start_block = 6;
  const size_t op_ex2_num_blocks = 1;
  const size_t part_num_blocks = 7;

  // Create the target partition data.
  string part_path;
  EXPECT_TRUE(utils::MakeTempFile(
      "SplitReplaceOrReplaceBzTest_part.XXXXXX", &part_path, nullptr));
  ScopedPathUnlinker part_path_unlinker(part_path);
  const size_t part_size = part_num_blocks * kBlockSize;
  chromeos::Blob part_data;
  if (compressible) {
    part_data.resize(part_size);
    test_utils::FillWithData(&part_data);
  } else {
    std::mt19937 gen(12345);
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    for (uint32_t i = 0; i < part_size; i++)
      part_data.push_back(dis(gen));
  }
  ASSERT_EQ(part_size, part_data.size());
  ASSERT_TRUE(utils::WriteFile(part_path.c_str(), part_data.data(), part_size));

  // Create original operation and blob data.
  const size_t op_ex1_offset = op_ex1_start_block * kBlockSize;
  const size_t op_ex1_size = op_ex1_num_blocks * kBlockSize;
  const size_t op_ex2_offset = op_ex2_start_block * kBlockSize;
  const size_t op_ex2_size = op_ex2_num_blocks * kBlockSize;
  DeltaArchiveManifest_InstallOperation op;
  op.set_type(orig_type);
  *(op.add_dst_extents()) = ExtentForRange(op_ex1_start_block,
                                           op_ex1_num_blocks);
  *(op.add_dst_extents()) = ExtentForRange(op_ex2_start_block,
                                           op_ex2_num_blocks);
  op.set_dst_length(op_ex1_num_blocks + op_ex2_num_blocks);

  chromeos::Blob op_data;
  op_data.insert(op_data.end(),
                 part_data.begin() + op_ex1_offset,
                 part_data.begin() + op_ex1_offset + op_ex1_size);
  op_data.insert(op_data.end(),
                 part_data.begin() + op_ex2_offset,
                 part_data.begin() + op_ex2_offset + op_ex2_size);
  chromeos::Blob op_blob;
  if (orig_type == DeltaArchiveManifest_InstallOperation_Type_REPLACE) {
    op_blob = op_data;
  } else {
    ASSERT_TRUE(BzipCompress(op_data, &op_blob));
  }
  op.set_data_offset(0);
  op.set_data_length(op_blob.size());

  AnnotatedOperation aop;
  aop.op = op;
  aop.name = "SplitTestOp";

  // Create the data file.
  string data_path;
  EXPECT_TRUE(utils::MakeTempFile(
      "SplitReplaceOrReplaceBzTest_data.XXXXXX", &data_path, nullptr));
  ScopedPathUnlinker data_path_unlinker(data_path);
  int data_fd = open(data_path.c_str(), O_RDWR, 000);
  EXPECT_GE(data_fd, 0);
  ScopedFdCloser data_fd_closer(&data_fd);
  EXPECT_TRUE(utils::WriteFile(data_path.c_str(), op_blob.data(),
                               op_blob.size()));
  off_t data_file_size = op_blob.size();
  BlobFileWriter blob_file(data_fd, &data_file_size);

  // Split the operation.
  vector<AnnotatedOperation> result_ops;
  ASSERT_TRUE(ABGenerator::SplitReplaceOrReplaceBz(
          aop, &result_ops, part_path, &blob_file));

  // Check the result.
  DeltaArchiveManifest_InstallOperation_Type expected_type =
      compressible ?
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ :
      DeltaArchiveManifest_InstallOperation_Type_REPLACE;

  ASSERT_EQ(2, result_ops.size());

  EXPECT_EQ("SplitTestOp:0", result_ops[0].name);
  DeltaArchiveManifest_InstallOperation first_op = result_ops[0].op;
  EXPECT_EQ(expected_type, first_op.type());
  EXPECT_EQ(op_ex1_size, first_op.dst_length());
  EXPECT_EQ(1, first_op.dst_extents().size());
  EXPECT_TRUE(ExtentEquals(first_op.dst_extents(0), op_ex1_start_block,
                           op_ex1_num_blocks));
  // Obtain the expected blob.
  chromeos::Blob first_expected_data(
      part_data.begin() + op_ex1_offset,
      part_data.begin() + op_ex1_offset + op_ex1_size);
  chromeos::Blob first_expected_blob;
  if (compressible) {
    ASSERT_TRUE(BzipCompress(first_expected_data, &first_expected_blob));
  } else {
    first_expected_blob = first_expected_data;
  }
  EXPECT_EQ(first_expected_blob.size(), first_op.data_length());
  // Check that the actual blob matches what's expected.
  chromeos::Blob first_data_blob(first_op.data_length());
  ssize_t bytes_read;
  ASSERT_TRUE(utils::PReadAll(data_fd,
                              first_data_blob.data(),
                              first_op.data_length(),
                              first_op.data_offset(),
                              &bytes_read));
  ASSERT_EQ(bytes_read, first_op.data_length());
  EXPECT_EQ(first_expected_blob, first_data_blob);

  EXPECT_EQ("SplitTestOp:1", result_ops[1].name);
  DeltaArchiveManifest_InstallOperation second_op = result_ops[1].op;
  EXPECT_EQ(expected_type, second_op.type());
  EXPECT_EQ(op_ex2_size, second_op.dst_length());
  EXPECT_EQ(1, second_op.dst_extents().size());
  EXPECT_TRUE(ExtentEquals(second_op.dst_extents(0), op_ex2_start_block,
                           op_ex2_num_blocks));
  // Obtain the expected blob.
  chromeos::Blob second_expected_data(
      part_data.begin() + op_ex2_offset,
      part_data.begin() + op_ex2_offset + op_ex2_size);
  chromeos::Blob second_expected_blob;
  if (compressible) {
    ASSERT_TRUE(BzipCompress(second_expected_data, &second_expected_blob));
  } else {
    second_expected_blob = second_expected_data;
  }
  EXPECT_EQ(second_expected_blob.size(), second_op.data_length());
  // Check that the actual blob matches what's expected.
  chromeos::Blob second_data_blob(second_op.data_length());
  ASSERT_TRUE(utils::PReadAll(data_fd,
                              second_data_blob.data(),
                              second_op.data_length(),
                              second_op.data_offset(),
                              &bytes_read));
  ASSERT_EQ(bytes_read, second_op.data_length());
  EXPECT_EQ(second_expected_blob, second_data_blob);

  // Check relative layout of data blobs.
  EXPECT_EQ(first_op.data_offset() + first_op.data_length(),
            second_op.data_offset());
  EXPECT_EQ(second_op.data_offset() + second_op.data_length(), data_file_size);
  // If we split a REPLACE into multiple ones, ensure reuse of preexisting blob.
  if (!compressible &&
      orig_type == DeltaArchiveManifest_InstallOperation_Type_REPLACE) {
    EXPECT_EQ(0, first_op.data_offset());
  }
}

// Tests merging of REPLACE/REPLACE_BZ operations.
void TestMergeReplaceOrReplaceBzOperations(
    DeltaArchiveManifest_InstallOperation_Type orig_type,
    bool compressible) {
  const size_t first_op_num_blocks = 1;
  const size_t second_op_num_blocks = 2;
  const size_t total_op_num_blocks = first_op_num_blocks + second_op_num_blocks;
  const size_t part_num_blocks = total_op_num_blocks + 2;

  // Create the target partition data.
  string part_path;
  EXPECT_TRUE(utils::MakeTempFile(
      "MergeReplaceOrReplaceBzTest_part.XXXXXX", &part_path, nullptr));
  ScopedPathUnlinker part_path_unlinker(part_path);
  const size_t part_size = part_num_blocks * kBlockSize;
  chromeos::Blob part_data;
  if (compressible) {
    part_data.resize(part_size);
    test_utils::FillWithData(&part_data);
  } else {
    std::mt19937 gen(12345);
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    for (uint32_t i = 0; i < part_size; i++)
      part_data.push_back(dis(gen));
  }
  ASSERT_EQ(part_size, part_data.size());
  ASSERT_TRUE(utils::WriteFile(part_path.c_str(), part_data.data(), part_size));

  // Create original operations and blob data.
  vector<AnnotatedOperation> aops;
  chromeos::Blob blob_data;
  const size_t total_op_size = total_op_num_blocks * kBlockSize;

  DeltaArchiveManifest_InstallOperation first_op;
  first_op.set_type(orig_type);
  const size_t first_op_size = first_op_num_blocks * kBlockSize;
  first_op.set_dst_length(first_op_size);
  *(first_op.add_dst_extents()) = ExtentForRange(0, first_op_num_blocks);
  chromeos::Blob first_op_data(part_data.begin(),
                               part_data.begin() + first_op_size);
  chromeos::Blob first_op_blob;
  if (orig_type == DeltaArchiveManifest_InstallOperation_Type_REPLACE) {
    first_op_blob = first_op_data;
  } else {
    ASSERT_TRUE(BzipCompress(first_op_data, &first_op_blob));
  }
  first_op.set_data_offset(0);
  first_op.set_data_length(first_op_blob.size());
  blob_data.insert(blob_data.end(), first_op_blob.begin(), first_op_blob.end());
  AnnotatedOperation first_aop;
  first_aop.op = first_op;
  first_aop.name = "first";
  aops.push_back(first_aop);

  DeltaArchiveManifest_InstallOperation second_op;
  second_op.set_type(orig_type);
  const size_t second_op_size = second_op_num_blocks * kBlockSize;
  second_op.set_dst_length(second_op_size);
  *(second_op.add_dst_extents()) = ExtentForRange(first_op_num_blocks,
                                                  second_op_num_blocks);
  chromeos::Blob second_op_data(part_data.begin() + first_op_size,
                                part_data.begin() + total_op_size);
  chromeos::Blob second_op_blob;
  if (orig_type == DeltaArchiveManifest_InstallOperation_Type_REPLACE) {
    second_op_blob = second_op_data;
  } else {
    ASSERT_TRUE(BzipCompress(second_op_data, &second_op_blob));
  }
  second_op.set_data_offset(first_op_blob.size());
  second_op.set_data_length(second_op_blob.size());
  blob_data.insert(blob_data.end(), second_op_blob.begin(),
                   second_op_blob.end());
  AnnotatedOperation second_aop;
  second_aop.op = second_op;
  second_aop.name = "second";
  aops.push_back(second_aop);

  // Create the data file.
  string data_path;
  EXPECT_TRUE(utils::MakeTempFile(
      "MergeReplaceOrReplaceBzTest_data.XXXXXX", &data_path, nullptr));
  ScopedPathUnlinker data_path_unlinker(data_path);
  int data_fd = open(data_path.c_str(), O_RDWR, 000);
  EXPECT_GE(data_fd, 0);
  ScopedFdCloser data_fd_closer(&data_fd);
  EXPECT_TRUE(utils::WriteFile(data_path.c_str(), blob_data.data(),
                               blob_data.size()));
  off_t data_file_size = blob_data.size();
  BlobFileWriter blob_file(data_fd, &data_file_size);

  // Merge the operations.
  EXPECT_TRUE(ABGenerator::MergeOperations(
      &aops, 5, part_path, &blob_file));

  // Check the result.
  DeltaArchiveManifest_InstallOperation_Type expected_op_type =
      compressible ?
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ :
      DeltaArchiveManifest_InstallOperation_Type_REPLACE;
  EXPECT_EQ(1, aops.size());
  DeltaArchiveManifest_InstallOperation new_op = aops[0].op;
  EXPECT_EQ(expected_op_type, new_op.type());
  EXPECT_FALSE(new_op.has_src_length());
  EXPECT_EQ(total_op_num_blocks * kBlockSize, new_op.dst_length());
  EXPECT_EQ(1, new_op.dst_extents().size());
  EXPECT_TRUE(ExtentEquals(new_op.dst_extents(0), 0, total_op_num_blocks));
  EXPECT_EQ("first,second", aops[0].name);

  // Check to see if the blob pointed to in the new extent has what we expect.
  chromeos::Blob expected_data(part_data.begin(),
                               part_data.begin() + total_op_size);
  chromeos::Blob expected_blob;
  if (compressible) {
    ASSERT_TRUE(BzipCompress(expected_data, &expected_blob));
  } else {
    expected_blob = expected_data;
  }
  ASSERT_EQ(expected_blob.size(), new_op.data_length());
  ASSERT_EQ(blob_data.size() + expected_blob.size(), data_file_size);
  chromeos::Blob new_op_blob(new_op.data_length());
  ssize_t bytes_read;
  ASSERT_TRUE(utils::PReadAll(data_fd,
                              new_op_blob.data(),
                              new_op.data_length(),
                              new_op.data_offset(),
                              &bytes_read));
  ASSERT_EQ(new_op.data_length(), bytes_read);
  EXPECT_EQ(expected_blob, new_op_blob);
}

}  // namespace

class ABGeneratorTest : public ::testing::Test {};

TEST_F(ABGeneratorTest, SplitSourceCopyTest) {
  DeltaArchiveManifest_InstallOperation op;
  op.set_type(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY);
  *(op.add_src_extents()) = ExtentForRange(2, 3);
  *(op.add_src_extents()) = ExtentForRange(6, 1);
  *(op.add_src_extents()) = ExtentForRange(8, 4);
  *(op.add_dst_extents()) = ExtentForRange(10, 2);
  *(op.add_dst_extents()) = ExtentForRange(14, 3);
  *(op.add_dst_extents()) = ExtentForRange(18, 3);

  AnnotatedOperation aop;
  aop.op = op;
  aop.name = "SplitSourceCopyTestOp";
  vector<AnnotatedOperation> result_ops;
  EXPECT_TRUE(ABGenerator::SplitSourceCopy(aop, &result_ops));
  EXPECT_EQ(result_ops.size(), 3);

  EXPECT_EQ("SplitSourceCopyTestOp:0", result_ops[0].name);
  DeltaArchiveManifest_InstallOperation first_op = result_ops[0].op;
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY,
            first_op.type());
  EXPECT_EQ(kBlockSize * 2, first_op.src_length());
  EXPECT_EQ(1, first_op.src_extents().size());
  EXPECT_EQ(2, first_op.src_extents(0).start_block());
  EXPECT_EQ(2, first_op.src_extents(0).num_blocks());
  EXPECT_EQ(kBlockSize * 2, first_op.dst_length());
  EXPECT_EQ(1, first_op.dst_extents().size());
  EXPECT_EQ(10, first_op.dst_extents(0).start_block());
  EXPECT_EQ(2, first_op.dst_extents(0).num_blocks());

  EXPECT_EQ("SplitSourceCopyTestOp:1", result_ops[1].name);
  DeltaArchiveManifest_InstallOperation second_op = result_ops[1].op;
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY,
            second_op.type());
  EXPECT_EQ(kBlockSize * 3, second_op.src_length());
  EXPECT_EQ(3, second_op.src_extents().size());
  EXPECT_EQ(4, second_op.src_extents(0).start_block());
  EXPECT_EQ(1, second_op.src_extents(0).num_blocks());
  EXPECT_EQ(6, second_op.src_extents(1).start_block());
  EXPECT_EQ(1, second_op.src_extents(1).num_blocks());
  EXPECT_EQ(8, second_op.src_extents(2).start_block());
  EXPECT_EQ(1, second_op.src_extents(2).num_blocks());
  EXPECT_EQ(kBlockSize * 3, second_op.dst_length());
  EXPECT_EQ(1, second_op.dst_extents().size());
  EXPECT_EQ(14, second_op.dst_extents(0).start_block());
  EXPECT_EQ(3, second_op.dst_extents(0).num_blocks());

  EXPECT_EQ("SplitSourceCopyTestOp:2", result_ops[2].name);
  DeltaArchiveManifest_InstallOperation third_op = result_ops[2].op;
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY,
            third_op.type());
  EXPECT_EQ(kBlockSize * 3, third_op.src_length());
  EXPECT_EQ(1, third_op.src_extents().size());
  EXPECT_EQ(9, third_op.src_extents(0).start_block());
  EXPECT_EQ(3, third_op.src_extents(0).num_blocks());
  EXPECT_EQ(kBlockSize * 3, third_op.dst_length());
  EXPECT_EQ(1, third_op.dst_extents().size());
  EXPECT_EQ(18, third_op.dst_extents(0).start_block());
  EXPECT_EQ(3, third_op.dst_extents(0).num_blocks());
}

TEST_F(ABGeneratorTest, SplitReplaceTest) {
  TestSplitReplaceOrReplaceBzOperation(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE, false);
}

TEST_F(ABGeneratorTest, SplitReplaceIntoReplaceBzTest) {
  TestSplitReplaceOrReplaceBzOperation(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE, true);
}

TEST_F(ABGeneratorTest, SplitReplaceBzTest) {
  TestSplitReplaceOrReplaceBzOperation(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ, true);
}

TEST_F(ABGeneratorTest, SplitReplaceBzIntoReplaceTest) {
  TestSplitReplaceOrReplaceBzOperation(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ, false);
}

TEST_F(ABGeneratorTest, SortOperationsByDestinationTest) {
  vector<AnnotatedOperation> aops;
  // One operation with multiple destination extents.
  DeltaArchiveManifest_InstallOperation first_op;
  *(first_op.add_dst_extents()) = ExtentForRange(6, 1);
  *(first_op.add_dst_extents()) = ExtentForRange(10, 2);
  AnnotatedOperation first_aop;
  first_aop.op = first_op;
  first_aop.name = "first";
  aops.push_back(first_aop);

  // One with no destination extent. Should end up at the end of the vector.
  DeltaArchiveManifest_InstallOperation second_op;
  AnnotatedOperation second_aop;
  second_aop.op = second_op;
  second_aop.name = "second";
  aops.push_back(second_aop);

  // One with one destination extent.
  DeltaArchiveManifest_InstallOperation third_op;
  *(third_op.add_dst_extents()) = ExtentForRange(3, 2);
  AnnotatedOperation third_aop;
  third_aop.op = third_op;
  third_aop.name = "third";
  aops.push_back(third_aop);

  ABGenerator::SortOperationsByDestination(&aops);
  EXPECT_EQ(aops.size(), 3);
  EXPECT_EQ(third_aop.name, aops[0].name);
  EXPECT_EQ(first_aop.name, aops[1].name);
  EXPECT_EQ(second_aop.name, aops[2].name);
}

TEST_F(ABGeneratorTest, MergeSourceCopyOperationsTest) {
  vector<AnnotatedOperation> aops;
  DeltaArchiveManifest_InstallOperation first_op;
  first_op.set_type(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY);
  first_op.set_src_length(kBlockSize);
  first_op.set_dst_length(kBlockSize);
  *(first_op.add_src_extents()) = ExtentForRange(1, 1);
  *(first_op.add_dst_extents()) = ExtentForRange(6, 1);
  AnnotatedOperation first_aop;
  first_aop.op = first_op;
  first_aop.name = "1";
  aops.push_back(first_aop);

  DeltaArchiveManifest_InstallOperation second_op;
  second_op.set_type(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY);
  second_op.set_src_length(3 * kBlockSize);
  second_op.set_dst_length(3 * kBlockSize);
  *(second_op.add_src_extents()) = ExtentForRange(2, 2);
  *(second_op.add_src_extents()) = ExtentForRange(8, 2);
  *(second_op.add_dst_extents()) = ExtentForRange(7, 3);
  *(second_op.add_dst_extents()) = ExtentForRange(11, 1);
  AnnotatedOperation second_aop;
  second_aop.op = second_op;
  second_aop.name = "2";
  aops.push_back(second_aop);

  DeltaArchiveManifest_InstallOperation third_op;
  third_op.set_type(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY);
  third_op.set_src_length(kBlockSize);
  third_op.set_dst_length(kBlockSize);
  *(third_op.add_src_extents()) = ExtentForRange(11, 1);
  *(third_op.add_dst_extents()) = ExtentForRange(12, 1);
  AnnotatedOperation third_aop;
  third_aop.op = third_op;
  third_aop.name = "3";
  aops.push_back(third_aop);

  BlobFileWriter blob_file(0, nullptr);
  EXPECT_TRUE(ABGenerator::MergeOperations(&aops, 5, "", &blob_file));

  EXPECT_EQ(aops.size(), 1);
  DeltaArchiveManifest_InstallOperation first_result_op = aops[0].op;
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY,
            first_result_op.type());
  EXPECT_EQ(kBlockSize * 5, first_result_op.src_length());
  EXPECT_EQ(3, first_result_op.src_extents().size());
  EXPECT_TRUE(ExtentEquals(first_result_op.src_extents(0), 1, 3));
  EXPECT_TRUE(ExtentEquals(first_result_op.src_extents(1), 8, 2));
  EXPECT_TRUE(ExtentEquals(first_result_op.src_extents(2), 11, 1));
  EXPECT_EQ(kBlockSize * 5, first_result_op.dst_length());
  EXPECT_EQ(2, first_result_op.dst_extents().size());
  EXPECT_TRUE(ExtentEquals(first_result_op.dst_extents(0), 6, 4));
  EXPECT_TRUE(ExtentEquals(first_result_op.dst_extents(1), 11, 2));
  EXPECT_EQ(aops[0].name, "1,2,3");
}

TEST_F(ABGeneratorTest, MergeReplaceOperationsTest) {
  TestMergeReplaceOrReplaceBzOperations(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE, false);
}

TEST_F(ABGeneratorTest, MergeReplaceOperationsToReplaceBzTest) {
  TestMergeReplaceOrReplaceBzOperations(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE, true);
}

TEST_F(ABGeneratorTest, MergeReplaceBzOperationsTest) {
  TestMergeReplaceOrReplaceBzOperations(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ, true);
}

TEST_F(ABGeneratorTest, MergeReplaceBzOperationsToReplaceTest) {
  TestMergeReplaceOrReplaceBzOperations(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ, false);
}

TEST_F(ABGeneratorTest, NoMergeOperationsTest) {
  // Test to make sure we don't merge operations that shouldn't be merged.
  vector<AnnotatedOperation> aops;
  DeltaArchiveManifest_InstallOperation first_op;
  first_op.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
  *(first_op.add_dst_extents()) = ExtentForRange(0, 1);
  first_op.set_data_length(kBlockSize);
  AnnotatedOperation first_aop;
  first_aop.op = first_op;
  aops.push_back(first_aop);

  // Should merge with first, except op types don't match...
  DeltaArchiveManifest_InstallOperation second_op;
  second_op.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE);
  *(second_op.add_dst_extents()) = ExtentForRange(1, 2);
  second_op.set_data_length(2 * kBlockSize);
  AnnotatedOperation second_aop;
  second_aop.op = second_op;
  aops.push_back(second_aop);

  // Should merge with second, except it would exceed chunk size...
  DeltaArchiveManifest_InstallOperation third_op;
  third_op.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE);
  *(third_op.add_dst_extents()) = ExtentForRange(3, 3);
  third_op.set_data_length(3 * kBlockSize);
  AnnotatedOperation third_aop;
  third_aop.op = third_op;
  aops.push_back(third_aop);

  // Should merge with third, except they aren't contiguous...
  DeltaArchiveManifest_InstallOperation fourth_op;
  fourth_op.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE);
  *(fourth_op.add_dst_extents()) = ExtentForRange(7, 2);
  fourth_op.set_data_length(2 * kBlockSize);
  AnnotatedOperation fourth_aop;
  fourth_aop.op = fourth_op;
  aops.push_back(fourth_aop);

  BlobFileWriter blob_file(0, nullptr);
  EXPECT_TRUE(ABGenerator::MergeOperations(&aops, 4, "", &blob_file));

  // No operations were merged, the number of ops is the same.
  EXPECT_EQ(aops.size(), 4);
}

}  // namespace chromeos_update_engine
