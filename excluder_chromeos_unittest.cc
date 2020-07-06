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

#include "update_engine/excluder_chromeos.h"

#include <memory>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "update_engine/common/prefs.h"

using std::string;
using std::unique_ptr;

namespace chromeos_update_engine {

constexpr char kDummyHash[] =
    "71ff43d76e2488e394e46872f5b066cc25e394c2c3e3790dd319517883b33db1";

class ExcluderChromeOSTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(tempdir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::PathExists(tempdir_.GetPath()));
    ASSERT_TRUE(prefs_.Init(tempdir_.GetPath()));
    excluder_ = std::make_unique<ExcluderChromeOS>(&prefs_);
  }

  base::ScopedTempDir tempdir_;
  Prefs prefs_;
  unique_ptr<ExcluderChromeOS> excluder_;
};

TEST_F(ExcluderChromeOSTest, ExclusionCheck) {
  EXPECT_FALSE(excluder_->IsExcluded(kDummyHash));
  EXPECT_TRUE(excluder_->Exclude(kDummyHash));
  EXPECT_TRUE(excluder_->IsExcluded(kDummyHash));
}

TEST_F(ExcluderChromeOSTest, ResetFlow) {
  EXPECT_TRUE(excluder_->Exclude("abc"));
  EXPECT_TRUE(excluder_->Exclude(kDummyHash));
  EXPECT_TRUE(excluder_->IsExcluded("abc"));
  EXPECT_TRUE(excluder_->IsExcluded(kDummyHash));

  EXPECT_TRUE(excluder_->Reset());
  EXPECT_FALSE(excluder_->IsExcluded("abc"));
  EXPECT_FALSE(excluder_->IsExcluded(kDummyHash));
}

}  // namespace chromeos_update_engine
