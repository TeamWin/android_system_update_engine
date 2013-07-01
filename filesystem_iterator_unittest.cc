// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <set>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "base/string_util.h"
#include <base/stringprintf.h>
#include "update_engine/filesystem_iterator.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class FilesystemIteratorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(utils::MakeTempDirectory("FilesystemIteratorTest-XXXXXX",
                                         &test_dir_));
    LOG(INFO) << "SetUp() mkdir " << test_dir_;
  }

  virtual void TearDown() {
    LOG(INFO) << "TearDown() rmdir " << test_dir_;
    EXPECT_EQ(0, System(StringPrintf("rm -rf %s", TestDir())));
  }

  const char* TestDir() {
    return test_dir_.c_str();
  }

 private:
  string test_dir_;
};

TEST_F(FilesystemIteratorTest, RunAsRootSuccessTest) {
  ASSERT_EQ(0, getuid());
  string first_image;
  ASSERT_TRUE(utils::MakeTempFile("FilesystemIteratorTest.image1-XXXXXX",
                                  &first_image, NULL));
  string sub_image;
  ASSERT_TRUE(utils::MakeTempFile("FilesystemIteratorTest.image2-XXXXXX",
                                  &sub_image, NULL));

  vector<string> expected_paths_vector;
  CreateExtImageAtPath(first_image, &expected_paths_vector);
  CreateExtImageAtPath(sub_image, NULL);
  ASSERT_EQ(0, System(string("mount -o loop ") + first_image + " " +
                      kMountPath));
  ASSERT_EQ(0, System(string("mount -o loop ") + sub_image + " " +
                      kMountPath + "/some_dir/mnt"));
  for (vector<string>::iterator it = expected_paths_vector.begin();
       it != expected_paths_vector.end(); ++it)
    *it = kMountPath + *it;
  set<string> expected_paths(expected_paths_vector.begin(),
                             expected_paths_vector.end());
  VerifyAllPaths(kMountPath, expected_paths);

  EXPECT_TRUE(utils::UnmountFilesystem(kMountPath + string("/some_dir/mnt")));
  EXPECT_TRUE(utils::UnmountFilesystem(kMountPath));
  EXPECT_EQ(0, System(string("rm -f ") + first_image + " " + sub_image));
}

TEST_F(FilesystemIteratorTest, NegativeTest) {
  {
    FilesystemIterator iter("/non/existent/path", set<string>());
    EXPECT_TRUE(iter.IsEnd());
    EXPECT_TRUE(iter.IsErr());
  }

  {
    FilesystemIterator iter(TestDir(), set<string>());
    EXPECT_FALSE(iter.IsEnd());
    EXPECT_FALSE(iter.IsErr());
    // Here I'm deleting the exact directory that iterator is point at,
    // then incrementing (which normally would descend into that directory).
    EXPECT_EQ(0, rmdir(TestDir()));
    iter.Increment();
    EXPECT_TRUE(iter.IsEnd());
    EXPECT_FALSE(iter.IsErr());
  }
}

TEST_F(FilesystemIteratorTest, DeleteWhileTraverseTest) {
  const string dir_name = TestDir();
  ASSERT_EQ(0, chmod(dir_name.c_str(), 0755));
  const string sub_dir_name(dir_name + "/a");
  ASSERT_EQ(0, mkdir(sub_dir_name.c_str(), 0755));
  const string sub_sub_dir_name(sub_dir_name + "/b");
  ASSERT_EQ(0, mkdir(sub_sub_dir_name.c_str(), 0755));
  ASSERT_EQ(0, mkdir((dir_name + "/b").c_str(), 0755));
  ASSERT_EQ(0, mkdir((dir_name + "/c").c_str(), 0755));

  string expected_paths_arr[] = {
    "",
    "/a",
    "/b",
    "/c"
  };
  set<string> expected_paths(expected_paths_arr,
                             expected_paths_arr +
                             arraysize(expected_paths_arr));

  FilesystemIterator iter(dir_name, set<string>());
  while (!iter.IsEnd()) {
    string path = iter.GetPartialPath();
    EXPECT_TRUE(expected_paths.find(path) != expected_paths.end());
    if (expected_paths.find(path) != expected_paths.end()) {
      expected_paths.erase(path);
    }
    if (path == "/a") {
      EXPECT_EQ(0, rmdir(sub_sub_dir_name.c_str()));
      EXPECT_EQ(0, rmdir(sub_dir_name.c_str()));
    }
    iter.Increment();
  }
  EXPECT_FALSE(iter.IsErr());
  EXPECT_TRUE(expected_paths.empty());
}

}  // namespace chromeos_update_engine
