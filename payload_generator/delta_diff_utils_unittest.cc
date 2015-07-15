// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/delta_diff_utils.h"

#include <algorithm>
#include <string>
#include <vector>

#include <base/files/scoped_file.h>
#include <gtest/gtest.h>

#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/extent_ranges.h"
#include "update_engine/payload_generator/extent_utils.h"
#include "update_engine/payload_generator/fake_filesystem.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::unique_ptr;
using std::vector;

namespace chromeos_update_engine {

namespace {

// Writes the |data| in the blocks specified by |extents| on the partition
// |part_path|. The |data| size could be smaller than the size of the blocks
// passed.
bool WriteExtents(const string& part_path,
                  const vector<Extent>& extents,
                  off_t block_size,
                  const chromeos::Blob& data) {
  uint64_t offset = 0;
  base::ScopedFILE fp(fopen(part_path.c_str(), "r+"));
  TEST_AND_RETURN_FALSE(fp.get());

  for (const Extent& extent : extents) {
    if (offset >= data.size())
      break;
    TEST_AND_RETURN_FALSE(
        fseek(fp.get(), extent.start_block() * block_size, SEEK_SET) == 0);
    uint64_t to_write = std::min(extent.num_blocks() * block_size,
                                 data.size() - offset);
    TEST_AND_RETURN_FALSE(
        fwrite(data.data() + offset, 1, to_write, fp.get()) == to_write);
    offset += extent.num_blocks() * block_size;
  }
  return true;
}

}  // namespace

class DeltaDiffUtilsTest : public ::testing::Test {
 protected:
  const uint64_t kFilesystemSize = kBlockSize * 1024;

  void SetUp() override {
    old_part_path_ = "DeltaDiffUtilsTest-old_part_path-XXXXXX";
    CreateFilesystem(&old_fs_, &old_part_path_, kFilesystemSize);

    new_part_path_ = "DeltaDiffUtilsTest-new_part_path-XXXXXX";
    CreateFilesystem(&old_fs_, &new_part_path_, kFilesystemSize);
  }

  void TearDown() override {
    unlink(old_part_path_.c_str());
    unlink(new_part_path_.c_str());
  }

  // Create a fake filesystem of the given size and initialize the partition
  // holding it.
  void CreateFilesystem(unique_ptr<FakeFilesystem>* fs, string* filename,
                        uint64_t size) {
    string pattern = *filename;
    ASSERT_TRUE(utils::MakeTempFile(pattern.c_str(), filename, nullptr));
    ASSERT_EQ(0, truncate(filename->c_str(), size));
    fs->reset(new FakeFilesystem(kBlockSize, size / kBlockSize));
  }

  // Paths to old and new temporary filesystems used in the tests.
  string old_part_path_;
  string new_part_path_;

  // FilesystemInterface fake implementations used to mock out the file/block
  // distribution.
  unique_ptr<FakeFilesystem> old_fs_;
  unique_ptr<FakeFilesystem> new_fs_;
};

TEST_F(DeltaDiffUtilsTest, MoveSmallTest) {
  chromeos::Blob data_blob(kBlockSize);
  test_utils::FillWithData(&data_blob);

  // The old file is on a different block than the new one.
  vector<Extent> old_extents = { ExtentForRange(11, 1) };
  vector<Extent> new_extents = { ExtentForRange(1, 1) };

  EXPECT_TRUE(WriteExtents(old_part_path_, old_extents, kBlockSize, data_blob));
  EXPECT_TRUE(WriteExtents(new_part_path_, new_extents, kBlockSize, data_blob));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(diff_utils::ReadExtentsToDiff(
      old_part_path_,
      new_part_path_,
      old_extents,
      new_extents,
      true,  // bsdiff_allowed
      &data,
      &op,
      false));  // src_ops_allowed
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_MOVE, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());
  EXPECT_EQ(1, op.src_extents_size());
  EXPECT_EQ(kBlockSize, op.src_length());
  EXPECT_EQ(1, op.dst_extents_size());
  EXPECT_EQ(kBlockSize, op.dst_length());
  EXPECT_EQ(BlocksInExtents(op.src_extents()),
            BlocksInExtents(op.dst_extents()));
  EXPECT_EQ(1, BlocksInExtents(op.dst_extents()));
}

TEST_F(DeltaDiffUtilsTest, MoveWithSameBlock) {
  // Setup the old/new files so that it has immobile chunks; we make sure to
  // utilize all sub-cases of such chunks: blocks 21--22 induce a split (src)
  // and complete removal (dst), whereas blocks 24--25 induce trimming of the
  // tail (src) and head (dst) of extents. The final block (29) is used for
  // ensuring we properly account for the number of bytes removed in cases where
  // the last block is partly filled. The detailed configuration:
  //
  // Old:  [ 20     21 22     23     24 25 ] [ 28     29 ]
  // New:  [ 18 ] [ 21 22 ] [ 20 ] [ 24 25     26 ] [ 29 ]
  // Same:          ^^ ^^            ^^ ^^            ^^
  vector<Extent> old_extents = {
      ExtentForRange(20, 6),
      ExtentForRange(28, 2) };
  vector<Extent> new_extents = {
      ExtentForRange(18, 1),
      ExtentForRange(21, 2),
      ExtentForRange(20, 1),
      ExtentForRange(24, 3),
      ExtentForRange(29, 1) };

  uint64_t num_blocks = BlocksInExtents(old_extents);
  EXPECT_EQ(num_blocks, BlocksInExtents(new_extents));

  // The size of the data should match the total number of blocks. Each block
  // has a different content.
  chromeos::Blob file_data;
  for (uint64_t i = 0; i < num_blocks; ++i) {
    file_data.resize(file_data.size() + kBlockSize, 'a' + i);
  }

  EXPECT_TRUE(WriteExtents(old_part_path_, old_extents, kBlockSize, file_data));
  EXPECT_TRUE(WriteExtents(new_part_path_, new_extents, kBlockSize, file_data));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(diff_utils::ReadExtentsToDiff(
      old_part_path_,
      new_part_path_,
      old_extents,
      new_extents,
      true,  // bsdiff_allowed
      &data,
      &op,
      false));  // src_ops_allowed

  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_MOVE, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());

  // The expected old and new extents that actually moved. See comment above.
  old_extents = {
      ExtentForRange(20, 1),
      ExtentForRange(23, 1),
      ExtentForRange(28, 1) };
  new_extents = {
      ExtentForRange(18, 1),
      ExtentForRange(20, 1),
      ExtentForRange(26, 1) };
  num_blocks = BlocksInExtents(old_extents);

  EXPECT_EQ(num_blocks * kBlockSize, op.src_length());
  EXPECT_EQ(num_blocks * kBlockSize, op.dst_length());

  EXPECT_EQ(old_extents.size(), op.src_extents_size());
  for (int i = 0; i < op.src_extents_size(); i++) {
    EXPECT_EQ(old_extents[i].start_block(), op.src_extents(i).start_block())
        << "i == " << i;
    EXPECT_EQ(old_extents[i].num_blocks(), op.src_extents(i).num_blocks())
        << "i == " << i;
  }

  EXPECT_EQ(new_extents.size(), op.dst_extents_size());
  for (int i = 0; i < op.dst_extents_size(); i++) {
    EXPECT_EQ(new_extents[i].start_block(), op.dst_extents(i).start_block())
        << "i == " << i;
    EXPECT_EQ(new_extents[i].num_blocks(), op.dst_extents(i).num_blocks())
        << "i == " << i;
  }
}

TEST_F(DeltaDiffUtilsTest, BsdiffSmallTest) {
  // Test a BSDIFF operation from block 1 to block 2.
  chromeos::Blob data_blob(kBlockSize);
  test_utils::FillWithData(&data_blob);

  // The old file is on a different block than the new one.
  vector<Extent> old_extents = { ExtentForRange(1, 1) };
  vector<Extent> new_extents = { ExtentForRange(2, 1) };

  EXPECT_TRUE(WriteExtents(old_part_path_, old_extents, kBlockSize, data_blob));
  // Modify one byte in the new file.
  data_blob[0]++;
  EXPECT_TRUE(WriteExtents(new_part_path_, new_extents, kBlockSize, data_blob));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(diff_utils::ReadExtentsToDiff(
      old_part_path_,
      new_part_path_,
      old_extents,
      new_extents,
      true,  // bsdiff_allowed
      &data,
      &op,
      false));  // src_ops_allowed

  EXPECT_FALSE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_BSDIFF, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());
  EXPECT_EQ(1, op.src_extents_size());
  EXPECT_EQ(kBlockSize, op.src_length());
  EXPECT_EQ(1, op.dst_extents_size());
  EXPECT_EQ(kBlockSize, op.dst_length());
  EXPECT_EQ(BlocksInExtents(op.src_extents()),
            BlocksInExtents(op.dst_extents()));
  EXPECT_EQ(1, BlocksInExtents(op.dst_extents()));
}

TEST_F(DeltaDiffUtilsTest, BsdiffNotAllowedTest) {
  // Same setup as the previous test, but this time BSDIFF operations are not
  // allowed.
  chromeos::Blob data_blob(kBlockSize);
  test_utils::FillWithData(&data_blob);

  // The old file is on a different block than the new one.
  vector<Extent> old_extents = { ExtentForRange(1, 1) };
  vector<Extent> new_extents = { ExtentForRange(2, 1) };

  EXPECT_TRUE(WriteExtents(old_part_path_, old_extents, kBlockSize, data_blob));
  // Modify one byte in the new file.
  data_blob[0]++;
  EXPECT_TRUE(WriteExtents(new_part_path_, new_extents, kBlockSize, data_blob));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(diff_utils::ReadExtentsToDiff(
      old_part_path_,
      new_part_path_,
      old_extents,
      new_extents,
      false,  // bsdiff_allowed
      &data,
      &op,
      false));  // src_ops_allowed

  EXPECT_FALSE(data.empty());

  // The point of this test is that we don't use BSDIFF the way the above
  // did. The rest of the details are to be caught in other tests.
  EXPECT_TRUE(op.has_type());
  EXPECT_NE(DeltaArchiveManifest_InstallOperation_Type_BSDIFF, op.type());
}

TEST_F(DeltaDiffUtilsTest, BsdiffNotAllowedMoveTest) {
  chromeos::Blob data_blob(kBlockSize);
  test_utils::FillWithData(&data_blob);

  // The old file is on a different block than the new one.
  vector<Extent> old_extents = { ExtentForRange(1, 1) };
  vector<Extent> new_extents = { ExtentForRange(2, 1) };

  EXPECT_TRUE(WriteExtents(old_part_path_, old_extents, kBlockSize, data_blob));
  EXPECT_TRUE(WriteExtents(new_part_path_, new_extents, kBlockSize, data_blob));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(diff_utils::ReadExtentsToDiff(
      old_part_path_,
      new_part_path_,
      old_extents,
      new_extents,
      false,  // bsdiff_allowed
      &data,
      &op,
      false));  // src_ops_allowed

  EXPECT_TRUE(data.empty());

  // The point of this test is that we can still use a MOVE for a file
  // that is blacklisted.
  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_MOVE, op.type());
}

TEST_F(DeltaDiffUtilsTest, ReplaceSmallTest) {
  // The old file is on a different block than the new one.
  vector<Extent> old_extents = { ExtentForRange(1, 1) };
  vector<Extent> new_extents = { ExtentForRange(2, 1) };

  // Make a blob that's just 1's that will compress well.
  chromeos::Blob ones(kBlockSize, 1);

  // Make a blob with random data that won't compress well.
  chromeos::Blob random_data;
  std::mt19937 gen(12345);
  std::uniform_int_distribution<uint8_t> dis(0, 255);
  for (uint32_t i = 0; i < kBlockSize; i++) {
    random_data.push_back(dis(gen));
  }

  for (int i = 0; i < 2; i++) {
    chromeos::Blob data_to_test = i == 0 ? random_data : ones;
    // The old_extents will be initialized with 0.
    EXPECT_TRUE(WriteExtents(new_part_path_, new_extents, kBlockSize,
                             data_to_test));

    chromeos::Blob data;
    DeltaArchiveManifest_InstallOperation op;
    EXPECT_TRUE(diff_utils::ReadExtentsToDiff(
        old_part_path_,
        new_part_path_,
        old_extents,
        new_extents,
        true,  // bsdiff_allowed
        &data,
        &op,
        false));  // src_ops_allowed
    EXPECT_FALSE(data.empty());

    EXPECT_TRUE(op.has_type());
    const DeltaArchiveManifest_InstallOperation_Type expected_type =
        (i == 0 ? DeltaArchiveManifest_InstallOperation_Type_REPLACE :
         DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
    EXPECT_EQ(expected_type, op.type());
    EXPECT_FALSE(op.has_data_offset());
    EXPECT_FALSE(op.has_data_length());
    EXPECT_EQ(0, op.src_extents_size());
    EXPECT_FALSE(op.has_src_length());
    EXPECT_EQ(1, op.dst_extents_size());
    EXPECT_EQ(data_to_test.size(), op.dst_length());
    EXPECT_EQ(1, BlocksInExtents(op.dst_extents()));
  }
}

TEST_F(DeltaDiffUtilsTest, SourceCopyTest) {
  // Makes sure SOURCE_COPY operations are emitted whenever src_ops_allowed
  // is true. It is the same setup as MoveSmallTest, which checks that
  // the operation is well-formed.
  chromeos::Blob data_blob(kBlockSize);
  test_utils::FillWithData(&data_blob);

  // The old file is on a different block than the new one.
  vector<Extent> old_extents = { ExtentForRange(11, 1) };
  vector<Extent> new_extents = { ExtentForRange(1, 1) };

  EXPECT_TRUE(WriteExtents(old_part_path_, old_extents, kBlockSize, data_blob));
  EXPECT_TRUE(WriteExtents(new_part_path_, new_extents, kBlockSize, data_blob));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(diff_utils::ReadExtentsToDiff(
      old_part_path_,
      new_part_path_,
      old_extents,
      new_extents,
      true,  // bsdiff_allowed
      &data,
      &op,
      true));  // src_ops_allowed
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY, op.type());
}

TEST_F(DeltaDiffUtilsTest, SourceBsdiffTest) {
  // Makes sure SOURCE_BSDIFF operations are emitted whenever src_ops_allowed
  // is true. It is the same setup as BsdiffSmallTest, which checks
  // that the operation is well-formed.
  chromeos::Blob data_blob(kBlockSize);
  test_utils::FillWithData(&data_blob);

  // The old file is on a different block than the new one.
  vector<Extent> old_extents = { ExtentForRange(1, 1) };
  vector<Extent> new_extents = { ExtentForRange(2, 1) };

  EXPECT_TRUE(WriteExtents(old_part_path_, old_extents, kBlockSize, data_blob));
  // Modify one byte in the new file.
  data_blob[0]++;
  EXPECT_TRUE(WriteExtents(new_part_path_, new_extents, kBlockSize, data_blob));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(diff_utils::ReadExtentsToDiff(
      old_part_path_,
      new_part_path_,
      old_extents,
      new_extents,
      true,  // bsdiff_allowed
      &data,
      &op,
      true));  // src_ops_allowed

  EXPECT_FALSE(data.empty());
  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_BSDIFF,
            op.type());
}

TEST_F(DeltaDiffUtilsTest, IsNoopOperationTest) {
  DeltaArchiveManifest_InstallOperation op;
  op.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
  EXPECT_FALSE(diff_utils::IsNoopOperation(op));
  op.set_type(DeltaArchiveManifest_InstallOperation_Type_MOVE);
  EXPECT_TRUE(diff_utils::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(3, 2);
  *(op.add_dst_extents()) = ExtentForRange(3, 2);
  EXPECT_TRUE(diff_utils::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(7, 5);
  *(op.add_dst_extents()) = ExtentForRange(7, 5);
  EXPECT_TRUE(diff_utils::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(20, 2);
  *(op.add_dst_extents()) = ExtentForRange(20, 1);
  *(op.add_dst_extents()) = ExtentForRange(21, 1);
  EXPECT_TRUE(diff_utils::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(24, 1);
  *(op.add_dst_extents()) = ExtentForRange(25, 1);
  EXPECT_FALSE(diff_utils::IsNoopOperation(op));
}

TEST_F(DeltaDiffUtilsTest, FilterNoopOperations) {
  AnnotatedOperation aop1;
  aop1.op.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
  *(aop1.op.add_dst_extents()) = ExtentForRange(3, 2);
  aop1.name = "aop1";

  AnnotatedOperation aop2 = aop1;
  aop2.name = "aop2";

  AnnotatedOperation noop;
  noop.op.set_type(DeltaArchiveManifest_InstallOperation_Type_MOVE);
  *(noop.op.add_src_extents()) = ExtentForRange(3, 2);
  *(noop.op.add_dst_extents()) = ExtentForRange(3, 2);
  noop.name = "noop";

  vector<AnnotatedOperation> ops = {noop, aop1, noop, noop, aop2, noop};
  diff_utils::FilterNoopOperations(&ops);
  EXPECT_EQ(2u, ops.size());
  EXPECT_EQ("aop1", ops[0].name);
  EXPECT_EQ("aop2", ops[1].name);
}

}  // namespace chromeos_update_engine
