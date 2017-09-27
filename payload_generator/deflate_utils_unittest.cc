//
// Copyright (C) 2017 The Android Open Source Project
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

#include "update_engine/payload_generator/deflate_utils.h"

#include <unistd.h>

#include <algorithm>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_generator/extent_ranges.h"
#include "update_engine/payload_generator/extent_utils.h"

using std::vector;

namespace chromeos_update_engine {
namespace deflate_utils {

TEST(DeflateUtilsTest, ExtentsShiftTest) {
  vector<Extent> base_extents = {ExtentForRange(10, 10),
                                 ExtentForRange(30, 10),
                                 ExtentForRange(50, 10),
                                 ExtentForRange(70, 10),
                                 ExtentForRange(90, 10)};
  vector<Extent> over_extents = {ExtentForRange(2, 2),
                                 ExtentForRange(5, 2),
                                 ExtentForRange(7, 3),
                                 ExtentForRange(13, 10),
                                 ExtentForRange(25, 20),
                                 ExtentForRange(47, 3)};
  vector<Extent> out_over_extents = {ExtentForRange(12, 2),
                                     ExtentForRange(15, 2),
                                     ExtentForRange(17, 3),
                                     ExtentForRange(33, 7),
                                     ExtentForRange(50, 3),
                                     ExtentForRange(55, 5),
                                     ExtentForRange(70, 10),
                                     ExtentForRange(90, 5),
                                     ExtentForRange(97, 3)};
  EXPECT_TRUE(ShiftExtentsOverExtents(base_extents, &over_extents));
  EXPECT_EQ(over_extents, out_over_extents);

  // Failure case
  base_extents = {ExtentForRange(10, 10)};
  over_extents = {ExtentForRange(2, 12)};
  EXPECT_FALSE(ShiftExtentsOverExtents(base_extents, &over_extents));
}

}  // namespace deflate_utils
}  // namespace chromeos_update_engine
