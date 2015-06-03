// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/extent_utils.h"

#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "update_engine/extent_ranges.h"
#include "update_engine/payload_constants.h"

using std::vector;

namespace chromeos_update_engine {

class ExtentUtilsTest : public ::testing::Test {};

TEST(ExtentUtilsTest, AppendSparseToExtentsTest) {
  vector<Extent> extents;

  EXPECT_EQ(0, extents.size());
  AppendBlockToExtents(&extents, kSparseHole);
  EXPECT_EQ(1, extents.size());
  AppendBlockToExtents(&extents, 0);
  EXPECT_EQ(2, extents.size());
  AppendBlockToExtents(&extents, kSparseHole);
  AppendBlockToExtents(&extents, kSparseHole);

  ASSERT_EQ(3, extents.size());
  EXPECT_EQ(kSparseHole, extents[0].start_block());
  EXPECT_EQ(1, extents[0].num_blocks());
  EXPECT_EQ(0, extents[1].start_block());
  EXPECT_EQ(1, extents[1].num_blocks());
  EXPECT_EQ(kSparseHole, extents[2].start_block());
  EXPECT_EQ(2, extents[2].num_blocks());
}

TEST(ExtentUtilsTest, BlocksInExtentsTest) {
  {
    vector<Extent> extents;
    EXPECT_EQ(0, BlocksInExtents(extents));
    extents.push_back(ExtentForRange(0, 1));
    EXPECT_EQ(1, BlocksInExtents(extents));
    extents.push_back(ExtentForRange(23, 55));
    EXPECT_EQ(56, BlocksInExtents(extents));
    extents.push_back(ExtentForRange(1, 2));
    EXPECT_EQ(58, BlocksInExtents(extents));
  }
  {
    google::protobuf::RepeatedPtrField<Extent> extents;
    EXPECT_EQ(0, BlocksInExtents(extents));
    *extents.Add() = ExtentForRange(0, 1);
    EXPECT_EQ(1, BlocksInExtents(extents));
    *extents.Add() = ExtentForRange(23, 55);
    EXPECT_EQ(56, BlocksInExtents(extents));
    *extents.Add() = ExtentForRange(1, 2);
    EXPECT_EQ(58, BlocksInExtents(extents));
  }
}

}  // namespace chromeos_update_engine
