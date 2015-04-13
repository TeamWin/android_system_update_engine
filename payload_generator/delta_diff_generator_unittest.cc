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
#include <gtest/gtest.h>

#include "update_engine/delta_performer.h"
#include "update_engine/extent_ranges.h"
#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/extent_mapper.h"
#include "update_engine/payload_generator/graph_types.h"
#include "update_engine/payload_generator/graph_utils.h"
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

int64_t BlocksInExtents(
    const google::protobuf::RepeatedPtrField<Extent>& extents) {
  int64_t ret = 0;
  for (int i = 0; i < extents.size(); i++) {
    ret += extents.Get(i).num_blocks();
  }
  return ret;
}

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

TEST_F(DeltaDiffGeneratorTest, NormalizeExtentsTest) {
  vector<Extent> extents;
  AddExtent(0, 3, &extents);
  // Make sure it works when there's just one extent.
  DeltaDiffGenerator::NormalizeExtents(&extents);
  EXPECT_EQ(extents.size(), 1);
  EXPECT_EQ(extents[0], ExtentForRange(0, 3));
  AddExtent(3, 2, &extents);
  AddExtent(5, 1, &extents);
  AddExtent(8, 4, &extents);
  AddExtent(13, 1, &extents);
  AddExtent(14, 2, &extents);
  DeltaDiffGenerator::NormalizeExtents(&extents);
  EXPECT_EQ(extents.size(), 3);
  EXPECT_EQ(extents[0], ExtentForRange(0, 6));
  EXPECT_EQ(extents[1], ExtentForRange(8, 4));
  EXPECT_EQ(extents[2], ExtentForRange(13, 3));
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

  vector<AnnotatedOperation> result_ops;
  DeltaDiffGenerator::SplitSourceCopy(op, &result_ops);
  EXPECT_EQ(result_ops.size(), 3);

  DeltaArchiveManifest_InstallOperation first_op = result_ops[0].op;
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY,
            first_op.type());
  EXPECT_EQ(kBlockSize * 2, first_op.src_length());
  EXPECT_EQ(1, first_op.src_extents().size());
  EXPECT_EQ(2, first_op.src_extents().Get(0).start_block());
  EXPECT_EQ(2, first_op.src_extents().Get(0).num_blocks());
  EXPECT_EQ(kBlockSize * 2, first_op.dst_length());
  EXPECT_EQ(1, first_op.dst_extents().size());
  EXPECT_EQ(10, first_op.dst_extents().Get(0).start_block());
  EXPECT_EQ(2, first_op.dst_extents().Get(0).num_blocks());

  DeltaArchiveManifest_InstallOperation second_op = result_ops[1].op;
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY,
            second_op.type());
  EXPECT_EQ(kBlockSize * 3, second_op.src_length());
  EXPECT_EQ(3, second_op.src_extents().size());
  EXPECT_EQ(4, second_op.src_extents().Get(0).start_block());
  EXPECT_EQ(1, second_op.src_extents().Get(0).num_blocks());
  EXPECT_EQ(6, second_op.src_extents().Get(1).start_block());
  EXPECT_EQ(1, second_op.src_extents().Get(1).num_blocks());
  EXPECT_EQ(8, second_op.src_extents().Get(2).start_block());
  EXPECT_EQ(1, second_op.src_extents().Get(2).num_blocks());
  EXPECT_EQ(kBlockSize * 3, second_op.dst_length());
  EXPECT_EQ(1, second_op.dst_extents().size());
  EXPECT_EQ(14, second_op.dst_extents().Get(0).start_block());
  EXPECT_EQ(3, second_op.dst_extents().Get(0).num_blocks());

  DeltaArchiveManifest_InstallOperation third_op = result_ops[2].op;
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY,
            third_op.type());
  EXPECT_EQ(kBlockSize * 3, third_op.src_length());
  EXPECT_EQ(1, third_op.src_extents().size());
  EXPECT_EQ(9, third_op.src_extents().Get(0).start_block());
  EXPECT_EQ(3, third_op.src_extents().Get(0).num_blocks());
  EXPECT_EQ(kBlockSize * 3, third_op.dst_length());
  EXPECT_EQ(1, third_op.dst_extents().size());
  EXPECT_EQ(18, third_op.dst_extents().Get(0).start_block());
  EXPECT_EQ(3, third_op.dst_extents().Get(0).num_blocks());
}

}  // namespace chromeos_update_engine
