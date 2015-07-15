// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/prefs.h"

#include <inttypes.h>

#include <string>

#include <base/files/file_util.h>
#include <base/macros.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>

using std::string;

namespace chromeos_update_engine {

class PrefsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(base::CreateNewTempDirectory("auprefs", &prefs_dir_));
    ASSERT_TRUE(prefs_.Init(prefs_dir_));
  }

  void TearDown() override {
    base::DeleteFile(prefs_dir_, true);  // recursive
  }

  bool SetValue(const string& key, const string& value) {
    return base::WriteFile(prefs_dir_.Append(key), value.data(),
                           value.length()) == static_cast<int>(value.length());
  }

  base::FilePath prefs_dir_;
  Prefs prefs_;
};

TEST_F(PrefsTest, GetFileNameForKey) {
  const char kKey[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-";
  base::FilePath path;
  EXPECT_TRUE(prefs_.GetFileNameForKey(kKey, &path));
  EXPECT_EQ(prefs_dir_.Append(kKey).value(), path.value());
}

TEST_F(PrefsTest, GetFileNameForKeyBadCharacter) {
  base::FilePath path;
  EXPECT_FALSE(prefs_.GetFileNameForKey("ABC abc", &path));
}

TEST_F(PrefsTest, GetFileNameForKeyEmpty) {
  base::FilePath path;
  EXPECT_FALSE(prefs_.GetFileNameForKey("", &path));
}

TEST_F(PrefsTest, GetString) {
  const char kKey[] = "test-key";
  const string test_data = "test data";
  ASSERT_TRUE(SetValue(kKey, test_data));
  string value;
  EXPECT_TRUE(prefs_.GetString(kKey, &value));
  EXPECT_EQ(test_data, value);
}

TEST_F(PrefsTest, GetStringBadKey) {
  string value;
  EXPECT_FALSE(prefs_.GetString(",bad", &value));
}

TEST_F(PrefsTest, GetStringNonExistentKey) {
  string value;
  EXPECT_FALSE(prefs_.GetString("non-existent-key", &value));
}

TEST_F(PrefsTest, SetString) {
  const char kKey[] = "my_test_key";
  const char kValue[] = "some test value\non 2 lines";
  EXPECT_TRUE(prefs_.SetString(kKey, kValue));
  string value;
  EXPECT_TRUE(base::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ(kValue, value);
}

TEST_F(PrefsTest, SetStringBadKey) {
  const char kKey[] = ".no-dots";
  EXPECT_FALSE(prefs_.SetString(kKey, "some value"));
  EXPECT_FALSE(base::PathExists(prefs_dir_.Append(kKey)));
}

TEST_F(PrefsTest, SetStringCreateDir) {
  const char kKey[] = "a-test-key";
  const char kValue[] = "test value";
  base::FilePath subdir = prefs_dir_.Append("subdir1").Append("subdir2");
  EXPECT_TRUE(prefs_.Init(subdir));
  EXPECT_TRUE(prefs_.SetString(kKey, kValue));
  string value;
  EXPECT_TRUE(base::ReadFileToString(subdir.Append(kKey), &value));
  EXPECT_EQ(kValue, value);
}

TEST_F(PrefsTest, SetStringDirCreationFailure) {
  EXPECT_TRUE(prefs_.Init(base::FilePath("/dev/null")));
  const char kKey[] = "test-key";
  EXPECT_FALSE(prefs_.SetString(kKey, "test value"));
}

TEST_F(PrefsTest, SetStringFileCreationFailure) {
  const char kKey[] = "a-test-key";
  base::CreateDirectory(prefs_dir_.Append(kKey));
  EXPECT_FALSE(prefs_.SetString(kKey, "test value"));
  EXPECT_TRUE(base::DirectoryExists(prefs_dir_.Append(kKey)));
}

TEST_F(PrefsTest, GetInt64) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, " \n 25 \t "));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(25, value);
}

TEST_F(PrefsTest, GetInt64BadValue) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, "30a"));
  int64_t value;
  EXPECT_FALSE(prefs_.GetInt64(kKey, &value));
}

TEST_F(PrefsTest, GetInt64Max) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, base::StringPrintf("%" PRIi64, kint64max)));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(kint64max, value);
}

TEST_F(PrefsTest, GetInt64Min) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, base::StringPrintf("%" PRIi64, kint64min)));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(kint64min, value);
}

TEST_F(PrefsTest, GetInt64Negative) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, " \t -100 \n "));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(-100, value);
}

TEST_F(PrefsTest, GetInt64NonExistentKey) {
  int64_t value;
  EXPECT_FALSE(prefs_.GetInt64("random-key", &value));
}

TEST_F(PrefsTest, SetInt64) {
  const char kKey[] = "test_int";
  EXPECT_TRUE(prefs_.SetInt64(kKey, -123));
  string value;
  EXPECT_TRUE(base::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ("-123", value);
}

TEST_F(PrefsTest, SetInt64BadKey) {
  const char kKey[] = "s p a c e s";
  EXPECT_FALSE(prefs_.SetInt64(kKey, 20));
  EXPECT_FALSE(base::PathExists(prefs_dir_.Append(kKey)));
}

TEST_F(PrefsTest, SetInt64Max) {
  const char kKey[] = "test-max-int";
  EXPECT_TRUE(prefs_.SetInt64(kKey, kint64max));
  string value;
  EXPECT_TRUE(base::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ(base::StringPrintf("%" PRIi64, kint64max), value);
}

TEST_F(PrefsTest, SetInt64Min) {
  const char kKey[] = "test-min-int";
  EXPECT_TRUE(prefs_.SetInt64(kKey, kint64min));
  string value;
  EXPECT_TRUE(base::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ(base::StringPrintf("%" PRIi64, kint64min), value);
}

TEST_F(PrefsTest, GetBooleanFalse) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, " \n false \t "));
  bool value;
  EXPECT_TRUE(prefs_.GetBoolean(kKey, &value));
  EXPECT_FALSE(value);
}

TEST_F(PrefsTest, GetBooleanTrue) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, " \t true \n "));
  bool value;
  EXPECT_TRUE(prefs_.GetBoolean(kKey, &value));
  EXPECT_TRUE(value);
}

TEST_F(PrefsTest, GetBooleanBadValue) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, "1"));
  bool value;
  EXPECT_FALSE(prefs_.GetBoolean(kKey, &value));
}

TEST_F(PrefsTest, GetBooleanBadEmptyValue) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, ""));
  bool value;
  EXPECT_FALSE(prefs_.GetBoolean(kKey, &value));
}

TEST_F(PrefsTest, GetBooleanNonExistentKey) {
  bool value;
  EXPECT_FALSE(prefs_.GetBoolean("random-key", &value));
}

TEST_F(PrefsTest, SetBooleanTrue) {
  const char kKey[] = "test-bool";
  EXPECT_TRUE(prefs_.SetBoolean(kKey, true));
  string value;
  EXPECT_TRUE(base::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ("true", value);
}

TEST_F(PrefsTest, SetBooleanFalse) {
  const char kKey[] = "test-bool";
  EXPECT_TRUE(prefs_.SetBoolean(kKey, false));
  string value;
  EXPECT_TRUE(base::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ("false", value);
}

TEST_F(PrefsTest, SetBooleanBadKey) {
  const char kKey[] = "s p a c e s";
  EXPECT_FALSE(prefs_.SetBoolean(kKey, true));
  EXPECT_FALSE(base::PathExists(prefs_dir_.Append(kKey)));
}

TEST_F(PrefsTest, ExistsWorks) {
  const char kKey[] = "exists-key";

  // test that the key doesn't exist before we set it.
  EXPECT_FALSE(prefs_.Exists(kKey));

  // test that the key exists after we set it.
  ASSERT_TRUE(prefs_.SetInt64(kKey, 8));
  EXPECT_TRUE(prefs_.Exists(kKey));
}

TEST_F(PrefsTest, DeleteWorks) {
  const char kKey[] = "delete-key";

  // test that it's alright to delete a non-existent key.
  EXPECT_TRUE(prefs_.Delete(kKey));

  // delete the key after we set it.
  ASSERT_TRUE(prefs_.SetInt64(kKey, 0));
  EXPECT_TRUE(prefs_.Delete(kKey));

  // make sure it doesn't exist anymore.
  EXPECT_FALSE(prefs_.Exists(kKey));
}

}  // namespace chromeos_update_engine
