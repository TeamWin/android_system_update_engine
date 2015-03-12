// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/full_update_generator.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "update_engine/delta_performer.h"
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
    config_.chunk_size = 128 * 1024;
    config_.block_size = 4096;
  }

  PayloadGenerationConfig config_;
};


TEST_F(FullUpdateGeneratorTest, RunTest) {
  chromeos::Blob new_root(20 * 1024 * 1024);
  chromeos::Blob new_kern(16 * 1024 * 1024);
  FillWithData(&new_root);
  FillWithData(&new_kern);

  // Assume hashes take 2 MiB beyond the rootfs.
  config_.rootfs_partition_size = new_root.size();
  config_.target.rootfs_size = new_root.size() - 2 * 1024 * 1024;
  config_.target.kernel_size = new_kern.size();

  EXPECT_TRUE(utils::MakeTempFile("NewFullUpdateTest_R.XXXXXX",
                                  &config_.target.rootfs_part,
                                  nullptr));
  ScopedPathUnlinker rootfs_part_unlinker(config_.target.rootfs_part);
  EXPECT_TRUE(test_utils::WriteFileVector(config_.target.rootfs_part,
                                          new_root));

  EXPECT_TRUE(utils::MakeTempFile("NewFullUpdateTest_K.XXXXXX",
                                  &config_.target.kernel_part,
                                  nullptr));
  ScopedPathUnlinker kernel_path_unlinker(config_.target.kernel_part);
  EXPECT_TRUE(test_utils::WriteFileVector(config_.target.kernel_part,
                                          new_kern));

  string out_blobs_path;
  int out_blobs_fd;
  EXPECT_TRUE(utils::MakeTempFile("NewFullUpdateTest_D.XXXXXX",
                                  &out_blobs_path,
                                  &out_blobs_fd));
  ScopedPathUnlinker out_blobs_path_unlinker(out_blobs_path);
  ScopedFdCloser out_blobs_fd_closer(&out_blobs_fd);

  off_t out_blobs_length = 0;
  Graph graph;
  vector<DeltaArchiveManifest_InstallOperation> kernel_ops;
  vector<Vertex::Index> final_order;

  EXPECT_TRUE(FullUpdateGenerator::Run(config_,
                                       out_blobs_fd,
                                       &out_blobs_length,
                                       &graph,
                                       &kernel_ops,
                                       &final_order));
  int64_t target_rootfs_chucks =
      config_.target.rootfs_size / config_.chunk_size;
  EXPECT_EQ(target_rootfs_chucks, graph.size());
  EXPECT_EQ(target_rootfs_chucks, final_order.size());
  EXPECT_EQ(new_kern.size() / config_.chunk_size, kernel_ops.size());
  for (off_t i = 0; i < target_rootfs_chucks; ++i) {
    EXPECT_EQ(i, final_order[i]);
    EXPECT_EQ(1, graph[i].op.dst_extents_size());
    EXPECT_EQ(i * config_.chunk_size / config_.block_size,
              graph[i].op.dst_extents(0).start_block()) << "i = " << i;
    EXPECT_EQ(config_.chunk_size / config_.block_size,
              graph[i].op.dst_extents(0).num_blocks());
    if (graph[i].op.type() !=
        DeltaArchiveManifest_InstallOperation_Type_REPLACE) {
      EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ,
                graph[i].op.type());
    }
  }
}

}  // namespace chromeos_update_engine
