//
// Copyright (C) 2016 The Android Open Source Project
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

#include "update_engine/omaha_utils.h"

#include <gtest/gtest.h>
#include <vector>

namespace chromeos_update_engine {

class OmahaUtilsTest : public ::testing::Test {};

TEST(OmahaUtilsTest, EolDateTest) {
  // Supported values are converted back and forth properly.
  const std::vector<EolDate> tests = {kEolDateInvalid, -1, 0, 1};
  for (EolDate eol_date : tests) {
    EXPECT_EQ(eol_date, StringToEolDate(EolDateToString(eol_date)))
        << "The StringToEolDate() was " << EolDateToString(eol_date);
  }

  // Invalid values are assumed as "supported".
  EXPECT_EQ(kEolDateInvalid, StringToEolDate(""));
  EXPECT_EQ(kEolDateInvalid, StringToEolDate("hello, world!"));
}

}  // namespace chromeos_update_engine
