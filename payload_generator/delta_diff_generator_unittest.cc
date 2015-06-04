// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/delta_diff_generator.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <bzlib.h>
#include <chromeos/secure_blob.h>
#include <gtest/gtest.h>

#include "update_engine/bzip.h"
#include "update_engine/delta_performer.h"
#include "update_engine/extent_ranges.h"
#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/extent_mapper.h"
#include "update_engine/payload_generator/extent_utils.h"
#include "update_engine/payload_generator/graph_types.h"
#include "update_engine/subprocess.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using chromeos_update_engine::test_utils::kRandomString;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

typedef DeltaDiffGenerator::Block Block;

typedef bool (*GetExtentsWithChunk)(const string&, off_t, off_t,
                                    vector<Extent>*);
extern GetExtentsWithChunk get_extents_with_chunk_func;

namespace {

uint64_t AddExtent(uint64_t start_block, uint64_t num_blocks,
                   vector<Extent>* extents) {
  extents->push_back(ExtentForRange(start_block, num_blocks));
  return num_blocks;
}

// Used to provide a fake list of extents for a given path.
std::map<string, vector<Extent>> fake_file_extents;

// Fake function for GatherExtents in delta_diff_generator. This function
// returns whatever is specified in the global fake_file_extents variable.
bool FakeGetExtents(const string& path, off_t chunk_offset, off_t chunk_size,
                    vector<Extent>* out) {
  if (fake_file_extents.find(path) != fake_file_extents.end()) {
    *out = fake_file_extents[path];
    return true;
  } else {
    return false;
  }
}

// Inserts |data| at block |offset| in |result|. Fills in the remaining blocks
// so that there are |num_blocks| blocks in |result|.
bool MakePartition(uint64_t num_blocks, const string& data, off_t offset,
                   chromeos::Blob* result) {
  TEST_AND_RETURN_FALSE(
      static_cast<uint64_t>((kBlockSize * offset) + data.size()) <=
                            kBlockSize * num_blocks);
  chromeos::Blob out(kBlockSize * num_blocks, 0);
  chromeos::Blob::iterator start = out.begin() + (kBlockSize * offset);
  copy(data.begin(), data.end(), start);
  *result = out;
  return true;
}

// Copies from |target| to |result|. Gets the substring of |target| starting
// at block |start_block| of length |num_blocks| blocks and inserts it at block
// |block_offset| in |result|.
void UpdatePartition(const string& target, uint64_t start_block,
                     uint64_t num_blocks, off_t block_offset,
                     chromeos::Blob* result) {
  uint64_t num_insert = num_blocks * kBlockSize;
  off_t target_offset = block_offset * kBlockSize;

  const string target_substr = target.substr(kBlockSize * start_block,
                                             num_insert);
  ASSERT_EQ(target_substr.size(), num_insert);

  for (uint64_t i = 0; i < num_insert; i++) {
    result->at(target_offset + i) =
        static_cast<unsigned char>(target_substr[i]);
  }
}

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
  int data_fd = open(data_path.c_str(), O_RDWR, 000);
  EXPECT_GE(data_fd, 0);
  ScopedFdCloser data_fd_closer(&data_fd);
  EXPECT_TRUE(utils::WriteFile(data_path.c_str(), op_blob.data(),
                               op_blob.size()));
  off_t data_file_size = op_blob.size();

  // Split the operation.
  vector<AnnotatedOperation> result_ops;
  ASSERT_TRUE(DeltaDiffGenerator::SplitReplaceOrReplaceBz(
          aop, &result_ops, part_path, data_fd, &data_file_size));

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
  int data_fd = open(data_path.c_str(), O_RDWR, 000);
  EXPECT_GE(data_fd, 0);
  ScopedFdCloser data_fd_closer(&data_fd);
  EXPECT_TRUE(utils::WriteFile(data_path.c_str(), blob_data.data(),
                               blob_data.size()));
  off_t data_file_size = blob_data.size();

  // Merge the operations.
  EXPECT_TRUE(DeltaDiffGenerator::MergeOperations(
      &aops, 5 * kBlockSize, part_path, data_fd, &data_file_size));

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

class DeltaDiffGeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(utils::MakeTempFile("DeltaDiffGeneratorTest-old_path-XXXXXX",
                                    &old_path_, nullptr));
    ASSERT_TRUE(utils::MakeTempFile("DeltaDiffGeneratorTest-new_path-XXXXXX",
                                    &new_path_, nullptr));
    ASSERT_TRUE(utils::MakeTempFile(
        "DeltaDiffGeneratorTest-old_part_path-XXXXXX",
        &old_part_path_, nullptr));
    ASSERT_TRUE(utils::MakeTempFile(
        "DeltaDiffGeneratorTest-new_part_path-XXXXXX",
        &new_part_path_, nullptr));

    // Mock out the extent gathering function.
    orig_get_extents_with_chunk_func_ = get_extents_with_chunk_func;
    get_extents_with_chunk_func = FakeGetExtents;
    // Ensure we start with a clean list of fake extents.
    fake_file_extents.clear();
  }

  void TearDown() override {
    unlink(old_path_.c_str());
    unlink(new_path_.c_str());
    unlink(old_part_path_.c_str());
    unlink(new_part_path_.c_str());
    get_extents_with_chunk_func = orig_get_extents_with_chunk_func_;
  }

  // Updates the FakeGetExtents for old_path_ and new_path_ to match the current
  // file size.
  void UpdateFakeExtents() {
    UpdateFakeFileExtents(old_path_, kBlockSize);
    UpdateFakeFileExtents(new_path_, kBlockSize);
  }

  // Update the fake_file_extents for |path| using the file size of that file,
  // assuming a block size of |block_size| and an optional |start_block|.
  // The faked extent list will have only one extent with the list of all the
  // contiguous blocks.
  void UpdateFakeFileExtents(const string& path, size_t block_size,
                             size_t start_block) {
    size_t num_blocks = (utils::FileSize(path) + block_size - 1) / block_size;
    if (num_blocks == 0) {
      fake_file_extents[path] = vector<Extent>{};
    } else {
      fake_file_extents[path] =
          vector<Extent>{1, ExtentForRange(start_block, num_blocks)};
    }
  }

  void UpdateFakeFileExtents(const string& path, size_t block_size) {
    UpdateFakeFileExtents(path, block_size, 0);
  }

  // Paths to old and new temporary files used in all the tests.
  string old_path_;
  string new_path_;

  // Paths to old and new temporary filesystems used in the tests.
  string old_part_path_;
  string new_part_path_;

  GetExtentsWithChunk orig_get_extents_with_chunk_func_;
};

TEST_F(DeltaDiffGeneratorTest, BlockDefaultValues) {
  // Tests that a Block is initialized with the default values as a
  // Vertex::kInvalidIndex. This is required by the delta generators.
  DeltaDiffGenerator::Block block;
  EXPECT_EQ(Vertex::kInvalidIndex, block.reader);
  EXPECT_EQ(Vertex::kInvalidIndex, block.writer);
}

TEST_F(DeltaDiffGeneratorTest, MoveSmallTest) {
  const string random_string(reinterpret_cast<const char*>(kRandomString),
                             sizeof(kRandomString));
  EXPECT_TRUE(utils::WriteFile(old_path_.c_str(),
                               random_string.c_str(),
                               random_string.size()));
  EXPECT_TRUE(utils::WriteFile(new_path_.c_str(),
                               random_string.c_str(),
                               random_string.size()));

  chromeos::Blob old_part;
  chromeos::Blob new_part;
  EXPECT_TRUE(MakePartition(11, random_string, 10, &old_part));
  EXPECT_TRUE(MakePartition(1, random_string, 0, &new_part));

  EXPECT_TRUE(utils::WriteFile(old_part_path_.c_str(),
                               old_part.data(), old_part.size()));
  EXPECT_TRUE(utils::WriteFile(new_part_path_.c_str(),
                               new_part.data(), new_part.size()));

  // Force the old file to be on a different block.
  UpdateFakeFileExtents(old_path_, kBlockSize, 10);
  UpdateFakeFileExtents(new_path_, kBlockSize);

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;

  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_part_path_,
                                                 new_part_path_,
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 true,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true,  // gather_extents
                                                 false,  // src_ops_allowed
                                                 old_path_,
                                                 new_path_));
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

TEST_F(DeltaDiffGeneratorTest, MoveWithSameBlock) {
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
  vector<Extent> old_extents;
  uint64_t num_extents = 0;
  num_extents += AddExtent(20, 6, &old_extents);
  num_extents += AddExtent(28, 2, &old_extents);
  fake_file_extents[old_path_] = old_extents;
  vector<Extent> new_extents;
  AddExtent(18, 1, &new_extents);
  AddExtent(21, 2, &new_extents);
  AddExtent(20, 1, &new_extents);
  AddExtent(24, 3, &new_extents);
  AddExtent(29, 1, &new_extents);
  fake_file_extents[new_path_] = new_extents;

  // The size of the data should match the total number of blocks; the last
  // block is only partly filled.
  size_t file_len = 7 * 4096 + 3333;
  const string random_string(reinterpret_cast<const char*>(kRandomString),
                             sizeof(kRandomString));
  string random_data;
  while (random_data.size() < file_len)
    random_data += random_string;
  if (random_data.size() > file_len) {
    random_data.erase(file_len);
    random_data.insert(random_data.end(), (4096 - 3333), 0);
  }

  EXPECT_TRUE(utils::WriteFile(old_path_.c_str(),
                               random_data.c_str(), file_len));
  EXPECT_TRUE(utils::WriteFile(new_path_.c_str(),
                               random_data.c_str(), file_len));

  // Make partitions that match the extents and random_data.
  chromeos::Blob old_part;
  chromeos::Blob new_part;
  EXPECT_TRUE(MakePartition(30, "", 0, &old_part));
  EXPECT_TRUE(MakePartition(30, "", 0, &new_part));

  UpdatePartition(random_data, 0, 6, 20, &old_part);
  UpdatePartition(random_data, 6, 2, 28, &old_part);

  UpdatePartition(random_data, 0, 1, 18, &new_part);
  UpdatePartition(random_data, 1, 2, 21, &new_part);
  UpdatePartition(random_data, 3, 1, 20, &new_part);
  UpdatePartition(random_data, 4, 3, 24, &new_part);
  UpdatePartition(random_data, 7, 1, 29, &new_part);

  EXPECT_TRUE(utils::WriteFile(old_part_path_.c_str(),
                               old_part.data(), old_part.size()));
  EXPECT_TRUE(utils::WriteFile(new_part_path_.c_str(),
                               new_part.data(), new_part.size()));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_part_path_,
                                                 new_part_path_,
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 true,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true,  // gather_extents
                                                 false,  // src_ops_allowed
                                                 old_path_,
                                                 new_path_));

  // Adjust the old/new extents to remove duplicates.
  old_extents[0].set_num_blocks(1);
  Extent e;
  e.set_start_block(23);
  e.set_num_blocks(1);
  old_extents.insert(old_extents.begin() + 1, e);
  old_extents[2].set_num_blocks(1);
  new_extents.erase(new_extents.begin() + 1);
  new_extents[2].set_start_block(26);
  new_extents[2].set_num_blocks(1);
  new_extents.erase(new_extents.begin() + 3);
  num_extents -= 5;
  file_len -= 4 * 4096 + 3333;

  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_MOVE, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());

  EXPECT_EQ(file_len, op.src_length());
  EXPECT_EQ(num_extents, BlocksInExtents(op.src_extents()));
  EXPECT_EQ(old_extents.size(), op.src_extents_size());
  for (int i = 0; i < op.src_extents_size(); i++) {
    EXPECT_EQ(old_extents[i].start_block(), op.src_extents(i).start_block())
        << "i == " << i;
    EXPECT_EQ(old_extents[i].num_blocks(), op.src_extents(i).num_blocks())
        << "i == " << i;
  }

  EXPECT_EQ(file_len, op.dst_length());
  EXPECT_EQ(num_extents, BlocksInExtents(op.dst_extents()));
  EXPECT_EQ(new_extents.size(), op.dst_extents_size());
  for (int i = 0; i < op.dst_extents_size(); i++) {
    EXPECT_EQ(new_extents[i].start_block(), op.dst_extents(i).start_block())
        << "i == " << i;
    EXPECT_EQ(new_extents[i].num_blocks(), op.dst_extents(i).num_blocks())
        << "i == " << i;
  }
}

TEST_F(DeltaDiffGeneratorTest, BsdiffSmallTest) {
  const string random_string(reinterpret_cast<const char*>(kRandomString),
                             sizeof(kRandomString));
  EXPECT_TRUE(utils::WriteFile(old_path_.c_str(),
                               random_string.c_str(),
                               random_string.size() - 1));
  EXPECT_TRUE(utils::WriteFile(new_path_.c_str(),
                               random_string.c_str(),
                               random_string.size()));
  UpdateFakeExtents();

  chromeos::Blob old_part;
  chromeos::Blob new_part;
  EXPECT_TRUE(MakePartition(
      1, random_string.substr(0, random_string.size() - 1),  0, &old_part));
  EXPECT_TRUE(MakePartition(1, random_string, 0, &new_part));

  EXPECT_TRUE(utils::WriteFile(old_part_path_.c_str(),
                               old_part.data(), old_part.size()));
  EXPECT_TRUE(utils::WriteFile(new_part_path_.c_str(),
                               new_part.data(), new_part.size()));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_part_path_,
                                                 new_part_path_,
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 true,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true,  // gather_extents
                                                 false,  // src_ops_allowed
                                                 old_path_,
                                                 new_path_));

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

TEST_F(DeltaDiffGeneratorTest, BsdiffNotAllowedTest) {
  const string random_string(reinterpret_cast<const char*>(kRandomString),
                             sizeof(kRandomString));
  EXPECT_TRUE(utils::WriteFile(old_path_.c_str(),
                               random_string.c_str(),
                               random_string.size() - 1));
  EXPECT_TRUE(utils::WriteFile(new_path_.c_str(),
                               random_string.c_str(),
                               random_string.size()));
  UpdateFakeExtents();

  chromeos::Blob old_part;
  chromeos::Blob new_part;
  EXPECT_TRUE(MakePartition(
      1, random_string.substr(0, random_string.size() - 1),  0, &old_part));
  EXPECT_TRUE(MakePartition(1, random_string, 0, &new_part));

  EXPECT_TRUE(utils::WriteFile(old_part_path_.c_str(),
                               old_part.data(), old_part.size()));
  EXPECT_TRUE(utils::WriteFile(new_part_path_.c_str(),
                               new_part.data(), new_part.size()));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;

  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_part_path_,
                                                 new_part_path_,
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 false,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true,  // gather_extents
                                                 false,  // src_ops_allowed
                                                 old_path_,
                                                 new_path_));

  EXPECT_FALSE(data.empty());

  // The point of this test is that we don't use BSDIFF the way the above
  // did. The rest of the details are to be caught in other tests.
  EXPECT_TRUE(op.has_type());
  EXPECT_NE(DeltaArchiveManifest_InstallOperation_Type_BSDIFF, op.type());
}

TEST_F(DeltaDiffGeneratorTest, BsdiffNotAllowedMoveTest) {
  const string random_string(reinterpret_cast<const char*>(kRandomString),
                             sizeof(kRandomString));
  EXPECT_TRUE(utils::WriteFile(old_path_.c_str(),
                               random_string.c_str(),
                               random_string.size()));
  EXPECT_TRUE(utils::WriteFile(new_path_.c_str(),
                               random_string.c_str(),
                               random_string.size()));
  UpdateFakeExtents();

  chromeos::Blob old_part;
  chromeos::Blob new_part;
  EXPECT_TRUE(MakePartition(1, random_string,  0, &old_part));
  EXPECT_TRUE(MakePartition(1, random_string, 0, &new_part));

  EXPECT_TRUE(utils::WriteFile(old_part_path_.c_str(),
                               old_part.data(), old_part.size()));
  EXPECT_TRUE(utils::WriteFile(new_part_path_.c_str(),
                               new_part.data(), new_part.size()));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;

  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_part_path_,
                                                 new_part_path_,
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 false,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true,  // gather_extents
                                                 false,  // src_ops_allowed
                                                 old_path_,
                                                 new_path_));

  EXPECT_TRUE(data.empty());

  // The point of this test is that we can still use a MOVE for a file
  // that is blacklisted.
  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_MOVE, op.type());
}

TEST_F(DeltaDiffGeneratorTest, ReplaceSmallTest) {
  chromeos::Blob new_part;

  chromeos::Blob old_part(kBlockSize, 0);
  EXPECT_TRUE(utils::WriteFile(old_part_path_.c_str(),
                               old_part.data(), old_part.size()));

  // Fill old_path with zeroes.
  chromeos::Blob old_data(kBlockSize, 0);
  EXPECT_TRUE(utils::WriteFile(old_path_.c_str(),
                               old_data.data(),
                               old_data.size()));

  chromeos::Blob random_data;
  // Make a blob with random data that won't compress well.
  std::mt19937 gen(12345);
  std::uniform_int_distribution<uint8_t> dis(0, 255);
  for (uint32_t i = 0; i < kBlockSize; i++) {
    random_data.push_back(dis(gen));
  }

  // Make a blob that's just 1's that will compress well.
  chromeos::Blob ones(kBlockSize, 1);

  for (int i = 0; i < 2; i++) {
    chromeos::Blob data_to_test = i == 0 ? random_data : ones;
    EXPECT_TRUE(utils::WriteFile(new_path_.c_str(),
                                 data_to_test.data(),
                                 data_to_test.size()));
    UpdateFakeExtents();

    string data_str(data_to_test.begin(), data_to_test.end());
    EXPECT_TRUE(MakePartition(1, data_str, 0, &new_part));
    EXPECT_TRUE(utils::WriteFile(new_part_path_.c_str(),
                                 new_part.data(), new_part.size()));

    chromeos::Blob data;
    DeltaArchiveManifest_InstallOperation op;
    EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_part_path_,
                                                   new_part_path_,
                                                   0,  // chunk_offset
                                                   -1,  // chunk_size
                                                   true,  // bsdiff_allowed
                                                   &data,
                                                   &op,
                                                   true,  // gather_extents
                                                   false,  // src_ops_allowed
                                                   old_path_,
                                                   new_path_));
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

TEST_F(DeltaDiffGeneratorTest, BsdiffNoGatherExtentsSmallTest) {
  const string random_string(reinterpret_cast<const char*>(kRandomString),
                             sizeof(kRandomString));
  EXPECT_TRUE(utils::WriteFile(old_path_.c_str(),
                               random_string.data(),
                               random_string.size() - 1));
  EXPECT_TRUE(utils::WriteFile(new_path_.c_str(),
                               random_string.c_str(),
                               random_string.size()));

  chromeos::Blob old_part;
  chromeos::Blob new_part;
  EXPECT_TRUE(MakePartition(
      1, random_string.substr(0, random_string.size() - 1),  0, &old_part));
  EXPECT_TRUE(MakePartition(1, random_string, 0, &new_part));

  EXPECT_TRUE(utils::WriteFile(old_part_path_.c_str(),
                               old_part.data(), old_part.size()));
  EXPECT_TRUE(utils::WriteFile(new_part_path_.c_str(),
                               new_part.data(), new_part.size()));
  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;

  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_part_path_,
                                                 new_part_path_,
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 true,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 false,  // gather_extents
                                                 false,  // src_ops_allowed
                                                 old_path_,
                                                 new_path_));
  EXPECT_FALSE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_BSDIFF, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());
  EXPECT_EQ(1, op.src_extents_size());
  EXPECT_EQ(0, op.src_extents().Get(0).start_block());
  EXPECT_EQ(1, op.src_extents().Get(0).num_blocks());
  EXPECT_EQ(kBlockSize, op.src_length());
  EXPECT_EQ(1, op.dst_extents_size());
  EXPECT_EQ(0, op.dst_extents().Get(0).start_block());
  EXPECT_EQ(1, op.dst_extents().Get(0).num_blocks());
  EXPECT_EQ(kBlockSize, op.dst_length());
}

TEST_F(DeltaDiffGeneratorTest, SourceCopyTest) {
  // Makes sure SOURCE_COPY operations are emitted whenever src_ops_allowed
  // is true. It is the same setup as MoveSmallTest, which checks that
  // the operation is well-formed.
  const string random_string(reinterpret_cast<const char*>(kRandomString),
                             sizeof(kRandomString));
  EXPECT_TRUE(utils::WriteFile(old_path_.c_str(),
                               random_string.data(),
                               random_string.size()));
  EXPECT_TRUE(utils::WriteFile(new_path_.c_str(),
                               random_string.c_str(),
                               random_string.size()));
  UpdateFakeExtents();

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;

  chromeos::Blob old_part;
  chromeos::Blob new_part;
  EXPECT_TRUE(MakePartition(1, random_string,  0, &old_part));
  EXPECT_TRUE(MakePartition(1, random_string, 0, &new_part));

  EXPECT_TRUE(utils::WriteFile(old_part_path_.c_str(),
                               old_part.data(), old_part.size()));
  EXPECT_TRUE(utils::WriteFile(new_part_path_.c_str(),
                               new_part.data(), new_part.size()));

  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_part_path_,
                                                 new_part_path_,
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 true,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true,  // gather_extents
                                                 true,  // src_ops_allowed
                                                 old_path_,
                                                 new_path_));
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY, op.type());
}

TEST_F(DeltaDiffGeneratorTest, SourceBsdiffTest) {
  // Makes sure SOURCE_BSDIFF operations are emitted whenever src_ops_allowed
  // is true. It is the same setup as BsdiffSmallTest, which checks
  // that the operation is well-formed.
  const string random_string(reinterpret_cast<const char*>(kRandomString),
                             sizeof(kRandomString));
  EXPECT_TRUE(utils::WriteFile(old_path_.c_str(),
                               random_string.data(),
                               random_string.size() - 1));
  EXPECT_TRUE(utils::WriteFile(new_path_.c_str(),
                               random_string.c_str(),
                               random_string.size()));
  UpdateFakeExtents();

  chromeos::Blob old_part;
  chromeos::Blob new_part;
  EXPECT_TRUE(MakePartition(
      1, random_string.substr(0, random_string.size() - 1),  0, &old_part));
  EXPECT_TRUE(MakePartition(1, random_string, 0, &new_part));

  EXPECT_TRUE(utils::WriteFile(old_part_path_.c_str(),
                               old_part.data(), old_part.size()));
  EXPECT_TRUE(utils::WriteFile(new_part_path_.c_str(),
                               new_part.data(), new_part.size()));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;

  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_part_path_,
                                                 new_part_path_,
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 true,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true,  // gather_extents
                                                 true,  // src_ops_allowed
                                                 old_path_,
                                                 new_path_));

  EXPECT_FALSE(data.empty());
  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_BSDIFF,
            op.type());
}

TEST_F(DeltaDiffGeneratorTest, ReorderBlobsTest) {
  string orig_blobs;
  EXPECT_TRUE(utils::MakeTempFile("ReorderBlobsTest.orig.XXXXXX", &orig_blobs,
                                  nullptr));

  string orig_data = "abcd";
  EXPECT_TRUE(
      utils::WriteFile(orig_blobs.c_str(), orig_data.data(), orig_data.size()));

  string new_blobs;
  EXPECT_TRUE(
      utils::MakeTempFile("ReorderBlobsTest.new.XXXXXX", &new_blobs, nullptr));

  DeltaArchiveManifest manifest;
  DeltaArchiveManifest_InstallOperation* op =
      manifest.add_install_operations();
  op->set_data_offset(1);
  op->set_data_length(3);
  op = manifest.add_install_operations();
  op->set_data_offset(0);
  op->set_data_length(1);

  EXPECT_TRUE(DeltaDiffGenerator::ReorderDataBlobs(&manifest,
                                                   orig_blobs,
                                                   new_blobs));

  string new_data;
  EXPECT_TRUE(utils::ReadFile(new_blobs, &new_data));
  EXPECT_EQ("bcda", new_data);
  EXPECT_EQ(2, manifest.install_operations_size());
  EXPECT_EQ(0, manifest.install_operations(0).data_offset());
  EXPECT_EQ(3, manifest.install_operations(0).data_length());
  EXPECT_EQ(3, manifest.install_operations(1).data_offset());
  EXPECT_EQ(1, manifest.install_operations(1).data_length());

  unlink(orig_blobs.c_str());
  unlink(new_blobs.c_str());
}

TEST_F(DeltaDiffGeneratorTest, IsNoopOperationTest) {
  DeltaArchiveManifest_InstallOperation op;
  op.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
  EXPECT_FALSE(DeltaDiffGenerator::IsNoopOperation(op));
  op.set_type(DeltaArchiveManifest_InstallOperation_Type_MOVE);
  EXPECT_TRUE(DeltaDiffGenerator::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(3, 2);
  *(op.add_dst_extents()) = ExtentForRange(3, 2);
  EXPECT_TRUE(DeltaDiffGenerator::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(7, 5);
  *(op.add_dst_extents()) = ExtentForRange(7, 5);
  EXPECT_TRUE(DeltaDiffGenerator::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(20, 2);
  *(op.add_dst_extents()) = ExtentForRange(20, 1);
  *(op.add_dst_extents()) = ExtentForRange(21, 1);
  EXPECT_TRUE(DeltaDiffGenerator::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(24, 1);
  *(op.add_dst_extents()) = ExtentForRange(25, 1);
  EXPECT_FALSE(DeltaDiffGenerator::IsNoopOperation(op));
}

TEST_F(DeltaDiffGeneratorTest, FilterNoopOperations) {
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
  DeltaDiffGenerator::FilterNoopOperations(&ops);
  EXPECT_EQ(2u, ops.size());
  EXPECT_EQ("aop1", ops[0].name);
  EXPECT_EQ("aop2", ops[1].name);
}

TEST_F(DeltaDiffGeneratorTest, SparseHolesFilteredTest) {
  // Test to see that extents starting with a sparse hole are filtered out by
  // ClearSparseHoles.
  vector<Extent> extents;
  AddExtent(kSparseHole, 1, &extents);
  AddExtent(21, 2, &extents);
  AddExtent(kSparseHole, 3, &extents);
  AddExtent(29, 1, &extents);
  DeltaDiffGenerator::ClearSparseHoles(&extents);
  EXPECT_EQ(extents.size(), 2);
  EXPECT_EQ(extents[0], ExtentForRange(21, 2));
  EXPECT_EQ(extents[1], ExtentForRange(29, 1));
}

TEST_F(DeltaDiffGeneratorTest, SplitSourceCopyTest) {
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
  EXPECT_TRUE(DeltaDiffGenerator::SplitSourceCopy(aop, &result_ops));
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

TEST_F(DeltaDiffGeneratorTest, SplitReplaceTest) {
  TestSplitReplaceOrReplaceBzOperation(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE, false);
}

TEST_F(DeltaDiffGeneratorTest, SplitReplaceIntoReplaceBzTest) {
  TestSplitReplaceOrReplaceBzOperation(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE, true);
}

TEST_F(DeltaDiffGeneratorTest, SplitReplaceBzTest) {
  TestSplitReplaceOrReplaceBzOperation(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ, true);
}

TEST_F(DeltaDiffGeneratorTest, SplitReplaceBzIntoReplaceTest) {
  TestSplitReplaceOrReplaceBzOperation(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ, false);
}

TEST_F(DeltaDiffGeneratorTest, SortOperationsByDestinationTest) {
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

  DeltaDiffGenerator::SortOperationsByDestination(&aops);
  EXPECT_EQ(aops.size(), 3);
  EXPECT_EQ(third_aop.name, aops[0].name);
  EXPECT_EQ(first_aop.name, aops[1].name);
  EXPECT_EQ(second_aop.name, aops[2].name);
}

TEST_F(DeltaDiffGeneratorTest, MergeSourceCopyOperationsTest) {
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

  EXPECT_TRUE(DeltaDiffGenerator::MergeOperations(
      &aops, 5 * kBlockSize, "", 0, nullptr));

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

TEST_F(DeltaDiffGeneratorTest, MergeReplaceOperationsTest) {
  TestMergeReplaceOrReplaceBzOperations(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE, false);
}

TEST_F(DeltaDiffGeneratorTest, MergeReplaceOperationsToReplaceBzTest) {
  TestMergeReplaceOrReplaceBzOperations(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE, true);
}

TEST_F(DeltaDiffGeneratorTest, MergeReplaceBzOperationsTest) {
  TestMergeReplaceOrReplaceBzOperations(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ, true);
}

TEST_F(DeltaDiffGeneratorTest, MergeReplaceBzOperationsToReplaceTest) {
  TestMergeReplaceOrReplaceBzOperations(
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ, false);
}

TEST_F(DeltaDiffGeneratorTest, NoMergeOperationsTest) {
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

  EXPECT_TRUE(DeltaDiffGenerator::MergeOperations(&aops, 4 * kBlockSize,
                                                  "", 0, nullptr));

  // No operations were merged, the number of ops is the same.
  EXPECT_EQ(aops.size(), 4);
}

TEST_F(DeltaDiffGeneratorTest, ExtendExtentsTest) {
  DeltaArchiveManifest_InstallOperation first_op;
  *(first_op.add_src_extents()) = ExtentForRange(1, 1);
  *(first_op.add_src_extents()) = ExtentForRange(3, 1);

  DeltaArchiveManifest_InstallOperation second_op;
  *(second_op.add_src_extents()) = ExtentForRange(4, 2);
  *(second_op.add_src_extents()) = ExtentForRange(8, 2);

  DeltaDiffGenerator::ExtendExtents(first_op.mutable_src_extents(),
                                    second_op.src_extents());
  EXPECT_EQ(first_op.src_extents_size(), 3);
  EXPECT_TRUE(ExtentEquals(first_op.src_extents(0), 1, 1));
  EXPECT_TRUE(ExtentEquals(first_op.src_extents(1), 3, 3));
  EXPECT_TRUE(ExtentEquals(first_op.src_extents(2), 8, 2));
}

}  // namespace chromeos_update_engine
