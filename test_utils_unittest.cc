// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/test_utils.h"

#include <string>

#include <gtest/gtest.h>

using std::string;

namespace chromeos_update_engine {
namespace test_utils {

class TestUtilsTest : public ::testing::Test { };

TEST(UtilsTest, RecursiveUnlinkDirTest) {
  string first_dir_name;
  ASSERT_TRUE(utils::MakeTempDirectory("RecursiveUnlinkDirTest-a-XXXXXX",
                                       &first_dir_name));
  ASSERT_EQ(0, Chmod(first_dir_name, 0755));
  string second_dir_name;
  ASSERT_TRUE(utils::MakeTempDirectory("RecursiveUnlinkDirTest-b-XXXXXX",
                                       &second_dir_name));
  ASSERT_EQ(0, Chmod(second_dir_name, 0755));

  EXPECT_EQ(0, Symlink(string("../") + first_dir_name,
                       second_dir_name + "/link"));
  EXPECT_EQ(0, System(string("echo hi > ") + second_dir_name + "/file"));
  EXPECT_EQ(0, Mkdir(second_dir_name + "/dir", 0755));
  EXPECT_EQ(0, System(string("echo ok > ") + second_dir_name + "/dir/subfile"));
  EXPECT_TRUE(test_utils::RecursiveUnlinkDir(second_dir_name));
  EXPECT_TRUE(utils::FileExists(first_dir_name.c_str()));
  EXPECT_EQ(0, System(string("rm -rf ") + first_dir_name));
  EXPECT_FALSE(utils::FileExists(second_dir_name.c_str()));
  EXPECT_TRUE(test_utils::RecursiveUnlinkDir("/something/that/doesnt/exist"));
}

}  // namespace test_utils
}  // namespace chromeos_update_engine
