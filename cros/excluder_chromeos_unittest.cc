//
// Copyright (C) 2020 The Android Open Source Project
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

#include "update_engine/cros/excluder_chromeos.h"

#include <gtest/gtest.h>

#include "update_engine/cros/fake_system_state.h"

namespace chromeos_update_engine {

namespace {
constexpr char kFakeHash[] =
    "71ff43d76e2488e394e46872f5b066cc25e394c2c3e3790dd319517883b33db1";
}  // namespace

class ExcluderChromeOSTest : public ::testing::Test {
 protected:
  void SetUp() override { FakeSystemState::CreateInstance(); }

  ExcluderChromeOS excluder_;
};

TEST_F(ExcluderChromeOSTest, ExclusionCheck) {
  EXPECT_FALSE(excluder_.IsExcluded(kFakeHash));
  EXPECT_TRUE(excluder_.Exclude(kFakeHash));
  EXPECT_TRUE(excluder_.IsExcluded(kFakeHash));
}

TEST_F(ExcluderChromeOSTest, ResetFlow) {
  EXPECT_TRUE(excluder_.Exclude("abc"));
  EXPECT_TRUE(excluder_.Exclude(kFakeHash));
  EXPECT_TRUE(excluder_.IsExcluded("abc"));
  EXPECT_TRUE(excluder_.IsExcluded(kFakeHash));

  EXPECT_TRUE(excluder_.Reset());
  EXPECT_FALSE(excluder_.IsExcluded("abc"));
  EXPECT_FALSE(excluder_.IsExcluded(kFakeHash));
}

}  // namespace chromeos_update_engine
