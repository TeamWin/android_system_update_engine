// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/simple_key_value_store.h"

#include <map>
#include <string>

#include <gtest/gtest.h>
#include <base/files/file_util.h>
#include <base/strings/string_util.h>

#include "update_engine/test_utils.h"

using base::FilePath;
using base::ReadFileToString;
using std::map;
using std::string;

namespace chromeos_update_engine {

class KeyValueStoreTest : public ::testing::Test {
 protected:
  ScopedTempFile file_;
  KeyValueStore store_;  // KeyValueStore under test.
};

TEST_F(KeyValueStoreTest, CommentsAreIgnored) {
  string blob = "# comment\nA=B\n\n\n#another=comment\n\n";
  ASSERT_TRUE(WriteFileString(file_.GetPath(), blob));
  EXPECT_TRUE(store_.Load(file_.GetPath()));

  EXPECT_TRUE(store_.Save(file_.GetPath()));
  string read_blob;
  ASSERT_TRUE(ReadFileToString(FilePath(file_.GetPath()), &read_blob));
  EXPECT_EQ("A=B\n", read_blob);
}

TEST_F(KeyValueStoreTest, EmptyTest) {
  ASSERT_TRUE(WriteFileString(file_.GetPath(), ""));
  EXPECT_TRUE(store_.Load(file_.GetPath()));

  EXPECT_TRUE(store_.Save(file_.GetPath()));
  string read_blob;
  ASSERT_TRUE(ReadFileToString(FilePath(file_.GetPath()), &read_blob));
  EXPECT_EQ("", read_blob);
}

TEST_F(KeyValueStoreTest, LoadAndReloadTest) {
  string blob = "A=B\nC=\n=\nFOO=BAR=BAZ\nBAR=BAX\nMISSING=NEWLINE";
  ASSERT_TRUE(WriteFileString(file_.GetPath(), blob));
  EXPECT_TRUE(store_.Load(file_.GetPath()));

  map<string, string> expected = {
      {"A", "B"}, {"C", ""}, {"", ""}, {"FOO", "BAR=BAZ"}, {"BAR", "BAX"},
      {"MISSING", "NEWLINE"}};

  // Test expected values
  string value;
  for (auto& it : expected) {
    EXPECT_TRUE(store_.GetString(it.first, &value));
    EXPECT_EQ(it.second, value) << "Testing key: " << it.first;
  }

  // Save, load and test again.
  EXPECT_TRUE(store_.Save(file_.GetPath()));
  KeyValueStore new_store;
  EXPECT_TRUE(new_store.Load(file_.GetPath()));

  for (auto& it : expected) {
    EXPECT_TRUE(new_store.GetString(it.first, &value)) << "key: " << it.first;
    EXPECT_EQ(it.second, value) << "key: " << it.first;
  }
}

TEST_F(KeyValueStoreTest, SimpleBooleanTest) {
  bool result;
  EXPECT_FALSE(store_.GetBoolean("A", &result));

  store_.SetBoolean("A", true);
  EXPECT_TRUE(store_.GetBoolean("A", &result));
  EXPECT_TRUE(result);

  store_.SetBoolean("A", false);
  EXPECT_TRUE(store_.GetBoolean("A", &result));
  EXPECT_FALSE(result);
}

TEST_F(KeyValueStoreTest, BooleanParsingTest) {
  string blob = "TRUE=true\nfalse=false\nvar=false\nDONT_SHOUT=TRUE\n";
  WriteFileString(file_.GetPath(), blob);
  EXPECT_TRUE(store_.Load(file_.GetPath()));

  map<string, bool> expected = {
      {"TRUE", true}, {"false", false}, {"var", false}};
  bool value;
  EXPECT_FALSE(store_.GetBoolean("DONT_SHOUT", &value));
  string str_value;
  EXPECT_TRUE(store_.GetString("DONT_SHOUT", &str_value));

  // Test expected values
  for (auto& it : expected) {
    EXPECT_TRUE(store_.GetBoolean(it.first, &value)) << "key: " << it.first;
    EXPECT_EQ(it.second, value) << "key: " << it.first;
  }
}

}  // namespace chromeos_update_engine
