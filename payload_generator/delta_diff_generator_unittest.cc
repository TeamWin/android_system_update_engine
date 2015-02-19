// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/delta_diff_generator.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
#include "update_engine/payload_generator/topological_sort.h"
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
}  // namespace

class DeltaDiffGeneratorTest : public ::testing::Test {
 protected:
  const string& old_path() { return old_path_; }
  const string& new_path() { return new_path_; }

  void SetUp() override {
    ASSERT_TRUE(utils::MakeTempFile("DeltaDiffGeneratorTest-old_path-XXXXXX",
                                    &old_path_, nullptr));
    ASSERT_TRUE(utils::MakeTempFile("DeltaDiffGeneratorTest-new_path-XXXXXX",
                                    &new_path_, nullptr));
  }

  void TearDown() override {
    unlink(old_path().c_str());
    unlink(new_path().c_str());
  }

 private:
  string old_path_;
  string new_path_;
};

TEST_F(DeltaDiffGeneratorTest, RunAsRootMoveSmallTest) {
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 true,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true));
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_MOVE, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());
  EXPECT_EQ(1, op.src_extents_size());
  EXPECT_EQ(sizeof(kRandomString), op.src_length());
  EXPECT_EQ(1, op.dst_extents_size());
  EXPECT_EQ(sizeof(kRandomString), op.dst_length());
  EXPECT_EQ(BlocksInExtents(op.src_extents()),
            BlocksInExtents(op.dst_extents()));
  EXPECT_EQ(1, BlocksInExtents(op.dst_extents()));
}

std::map<string, vector<Extent>*> fake_file_extents;

bool FakeGetExtents(const string& path, off_t chunk_offset, off_t chunk_size,
                    vector<Extent>* out) {
  if (fake_file_extents.count(path) == 1) {
    *out = *fake_file_extents[path];
    return true;
  } else {
    return false;
  }
}

uint64_t AddExtent(uint64_t start_block, uint64_t num_blocks,
                   vector<Extent>* extents) {
  Extent e;
  e.set_start_block(start_block);
  e.set_num_blocks(num_blocks);
  extents->push_back(e);
  return num_blocks;
}

TEST_F(DeltaDiffGeneratorTest, RunAsRootMoveWithSameBlock) {
  // Mock out the extent gathering function.
  GetExtentsWithChunk orig_get_extents_with_chunk_func =
      get_extents_with_chunk_func;
  get_extents_with_chunk_func = FakeGetExtents;

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
  fake_file_extents[old_path()] = &old_extents;
  vector<Extent> new_extents;
  AddExtent(18, 1, &new_extents);
  AddExtent(21, 2, &new_extents);
  AddExtent(20, 1, &new_extents);
  AddExtent(24, 3, &new_extents);
  AddExtent(29, 1, &new_extents);
  fake_file_extents[new_path()] = &new_extents;

  // The size of the data should match the total number of blocks; the last
  // block is only partly filled.
  size_t file_len = 7 * 4096 + 3333;
  const string random_string(reinterpret_cast<const char*>(kRandomString),
                             sizeof(kRandomString));
  string random_data;
  while (random_data.size() < file_len)
    random_data += random_string;
  if (random_data.size() > file_len)
    random_data.erase(file_len);
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               random_data.c_str(), file_len));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               random_data.c_str(), file_len));

  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 true,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true));

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

  // Clean up fake extents and restore the module's extent gathering logic.
  fake_file_extents.clear();
  get_extents_with_chunk_func = orig_get_extents_with_chunk_func;
}

TEST_F(DeltaDiffGeneratorTest, RunAsRootBsdiffSmallTest) {
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString) - 1));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 true,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true));
  EXPECT_FALSE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_BSDIFF, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());
  EXPECT_EQ(1, op.src_extents_size());
  EXPECT_EQ(sizeof(kRandomString) - 1, op.src_length());
  EXPECT_EQ(1, op.dst_extents_size());
  EXPECT_EQ(sizeof(kRandomString), op.dst_length());
  EXPECT_EQ(BlocksInExtents(op.src_extents()),
            BlocksInExtents(op.dst_extents()));
  EXPECT_EQ(1, BlocksInExtents(op.dst_extents()));
}

TEST_F(DeltaDiffGeneratorTest, RunAsRootBsdiffNotAllowedTest) {
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString) - 1));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;

  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 false,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true));
  EXPECT_FALSE(data.empty());

  // The point of this test is that we don't use BSDIFF the way the above
  // did. The rest of the details are to be caught in other tests.
  EXPECT_TRUE(op.has_type());
  EXPECT_NE(DeltaArchiveManifest_InstallOperation_Type_BSDIFF, op.type());
}

TEST_F(DeltaDiffGeneratorTest, RunAsRootBsdiffNotAllowedMoveTest) {
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;

  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 false,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true));
  EXPECT_TRUE(data.empty());

  // The point of this test is that we can still use a MOVE for a file
  // that is blacklisted.
  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_MOVE, op.type());
}

TEST_F(DeltaDiffGeneratorTest, RunAsRootReplaceSmallTest) {
  chromeos::Blob new_data;
  for (int i = 0; i < 2; i++) {
    new_data.insert(new_data.end(),
                    std::begin(kRandomString), std::end(kRandomString));
    EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                                 new_data.data(),
                                 new_data.size()));
    chromeos::Blob data;
    DeltaArchiveManifest_InstallOperation op;
    EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                   new_path(),
                                                   0,  // chunk_offset
                                                   -1,  // chunk_size
                                                   true,  // bsdiff_allowed
                                                   &data,
                                                   &op,
                                                   true));
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
    EXPECT_EQ(new_data.size(), op.dst_length());
    EXPECT_EQ(1, BlocksInExtents(op.dst_extents()));
  }
}

TEST_F(DeltaDiffGeneratorTest, RunAsRootBsdiffNoGatherExtentsSmallTest) {
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString) - 1));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 0,  // chunk_offset
                                                 -1,  // chunk_size
                                                 true,  // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 false));
  EXPECT_FALSE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_BSDIFF, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());
  EXPECT_EQ(1, op.src_extents_size());
  EXPECT_EQ(0, op.src_extents().Get(0).start_block());
  EXPECT_EQ(1, op.src_extents().Get(0).num_blocks());
  EXPECT_EQ(sizeof(kRandomString) - 1, op.src_length());
  EXPECT_EQ(1, op.dst_extents_size());
  EXPECT_EQ(0, op.dst_extents().Get(0).start_block());
  EXPECT_EQ(1, op.dst_extents().Get(0).num_blocks());
  EXPECT_EQ(sizeof(kRandomString), op.dst_length());
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
  *(op.add_src_extents()) = ExtentForRange(kSparseHole, 2);
  *(op.add_src_extents()) = ExtentForRange(kSparseHole, 1);
  *(op.add_dst_extents()) = ExtentForRange(kSparseHole, 3);
  EXPECT_TRUE(DeltaDiffGenerator::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(24, 1);
  *(op.add_dst_extents()) = ExtentForRange(25, 1);
  EXPECT_FALSE(DeltaDiffGenerator::IsNoopOperation(op));
}

}  // namespace chromeos_update_engine
