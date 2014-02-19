// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/hwid_override.h"

#include <string>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

namespace chromeos_update_engine {

class HwidOverrideTest : public ::testing::Test {
 public:
  HwidOverrideTest() {}
  virtual ~HwidOverrideTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(tempdir_.CreateUniqueTempDir());
    ASSERT_TRUE(file_util::CreateDirectory(tempdir_.path().Append("etc")));
  }

 protected:
  base::ScopedTempDir tempdir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HwidOverrideTest);
};

TEST_F(HwidOverrideTest, ReadGood) {
  std::string expected_hwid("expected");
  std::string keyval(HwidOverride::kHwidOverrideKey);
  keyval += ("=" + expected_hwid);
  ASSERT_EQ(file_util::WriteFile(tempdir_.path().Append("etc/lsb-release"),
                                 keyval.c_str(), keyval.length()),
            keyval.length());
  EXPECT_EQ(expected_hwid, HwidOverride::Read(tempdir_.path()));
}

TEST_F(HwidOverrideTest, ReadNothing) {
  std::string keyval("SOMETHING_ELSE=UNINTERESTING");
  ASSERT_EQ(file_util::WriteFile(tempdir_.path().Append("etc/lsb-release"),
                                 keyval.c_str(), keyval.length()),
            keyval.length());
  EXPECT_EQ(std::string(), HwidOverride::Read(tempdir_.path()));
}

TEST_F(HwidOverrideTest, ReadFailure) {
  EXPECT_EQ(std::string(), HwidOverride::Read(tempdir_.path()));
}

}  // namespace chromeos_update_engine
