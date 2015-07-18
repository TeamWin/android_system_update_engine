// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/full_update_generator.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "update_engine/delta_performer.h"
#include "update_engine/payload_generator/extent_utils.h"
#include "update_engine/test_utils.h"

using chromeos_update_engine::test_utils::FillWithData;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class FullUpdateGeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.is_delta = false;
    config_.minor_version = DeltaPerformer::kFullPayloadMinorVersion;
    config_.hard_chunk_size = 128 * 1024;
    config_.block_size = 4096;

    EXPECT_TRUE(utils::MakeTempFile("FullUpdateTest_rootfs.XXXXXX",
                                    &config_.target.rootfs.path,
                                    nullptr));
    EXPECT_TRUE(utils::MakeTempFile("FullUpdateTest_kernel.XXXXXX",
                                    &config_.target.kernel.path,
                                    nullptr));
    EXPECT_TRUE(utils::MakeTempFile("FullUpdateTest_blobs.XXXXXX",
                                    &out_blobs_path_,
                                    &out_blobs_fd_));

    rootfs_part_unlinker_.reset(
        new ScopedPathUnlinker(config_.target.rootfs.path));
    kernel_part_unlinker_.reset(
        new ScopedPathUnlinker(config_.target.kernel.path));
    out_blobs_unlinker_.reset(new ScopedPathUnlinker(out_blobs_path_));
  }

  PayloadGenerationConfig config_;

  // Output file holding the payload blobs.
  string out_blobs_path_;
  int out_blobs_fd_{-1};
  ScopedFdCloser out_blobs_fd_closer_{&out_blobs_fd_};

  std::unique_ptr<ScopedPathUnlinker> rootfs_part_unlinker_;
  std::unique_ptr<ScopedPathUnlinker> kernel_part_unlinker_;
  std::unique_ptr<ScopedPathUnlinker> out_blobs_unlinker_;

  // FullUpdateGenerator under test.
  FullUpdateGenerator generator_;
};

TEST_F(FullUpdateGeneratorTest, RunTest) {
  chromeos::Blob new_root(9 * 1024 * 1024);
  chromeos::Blob new_kern(3 * 1024 * 1024);
  FillWithData(&new_root);
  FillWithData(&new_kern);

  // Assume hashes take 2 MiB beyond the rootfs.
  config_.rootfs_partition_size = new_root.size();
  config_.target.rootfs.size = new_root.size() - 2 * 1024 * 1024;
  config_.target.kernel.size = new_kern.size();

  EXPECT_TRUE(test_utils::WriteFileVector(config_.target.rootfs.path,
                                          new_root));
  EXPECT_TRUE(test_utils::WriteFileVector(config_.target.kernel.path,
                                          new_kern));

  off_t out_blobs_length = 0;
  vector<AnnotatedOperation> rootfs_ops;
  vector<AnnotatedOperation> kernel_ops;

  EXPECT_TRUE(generator_.GenerateOperations(config_,
                                            out_blobs_fd_,
                                            &out_blobs_length,
                                            &rootfs_ops,
                                            &kernel_ops));
  int64_t target_rootfs_chunks =
      config_.target.rootfs.size / config_.hard_chunk_size;
  EXPECT_EQ(target_rootfs_chunks, rootfs_ops.size());
  EXPECT_EQ(new_kern.size() / config_.hard_chunk_size, kernel_ops.size());
  for (off_t i = 0; i < target_rootfs_chunks; ++i) {
    EXPECT_EQ(1, rootfs_ops[i].op.dst_extents_size());
    EXPECT_EQ(i * config_.hard_chunk_size / config_.block_size,
              rootfs_ops[i].op.dst_extents(0).start_block()) << "i = " << i;
    EXPECT_EQ(config_.hard_chunk_size / config_.block_size,
              rootfs_ops[i].op.dst_extents(0).num_blocks());
    if (rootfs_ops[i].op.type() !=
        DeltaArchiveManifest_InstallOperation_Type_REPLACE) {
      EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ,
                rootfs_ops[i].op.type());
    }
  }
}

// Test that if the chunk size is not a divisor of the image size, it handles
// correctly the last chunk of each partition.
TEST_F(FullUpdateGeneratorTest, ChunkSizeTooBig) {
  config_.hard_chunk_size = 1024 * 1024;
  config_.soft_chunk_size = config_.hard_chunk_size;
  chromeos::Blob new_root(1536 * 1024);  // 1.5 MiB
  chromeos::Blob new_kern(128 * 1024);
  config_.rootfs_partition_size = new_root.size();
  config_.target.rootfs.size = new_root.size();
  config_.target.kernel.size = new_kern.size();

  EXPECT_TRUE(test_utils::WriteFileVector(config_.target.rootfs.path,
                                          new_root));
  EXPECT_TRUE(test_utils::WriteFileVector(config_.target.kernel.path,
                                          new_kern));

  off_t out_blobs_length = 0;
  vector<AnnotatedOperation> rootfs_ops;
  vector<AnnotatedOperation> kernel_ops;

  EXPECT_TRUE(generator_.GenerateOperations(config_,
                                            out_blobs_fd_,
                                            &out_blobs_length,
                                            &rootfs_ops,
                                            &kernel_ops));
  // rootfs has one chunk and a half.
  EXPECT_EQ(2, rootfs_ops.size());
  EXPECT_EQ(config_.hard_chunk_size / config_.block_size,
            BlocksInExtents(rootfs_ops[0].op.dst_extents()));
  EXPECT_EQ((new_root.size() - config_.hard_chunk_size) / config_.block_size,
            BlocksInExtents(rootfs_ops[1].op.dst_extents()));

  // kernel has less than one chunk.
  EXPECT_EQ(1, kernel_ops.size());
  EXPECT_EQ(new_kern.size() / config_.block_size,
            BlocksInExtents(kernel_ops[0].op.dst_extents()));
}

}  // namespace chromeos_update_engine
