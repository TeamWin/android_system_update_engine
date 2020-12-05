//
// Copyright (C) 2012 The Android Open Source Project
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

#include "update_engine/common/prefs.h"

#include <inttypes.h>

#include <limits>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using std::string;
using std::vector;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::UnorderedElementsAre;

namespace {
// Test key used along the tests.
const char kKey[] = "test-key";
}  // namespace

namespace chromeos_update_engine {

class BasePrefsTest : public ::testing::Test {
 protected:
  void MultiNamespaceKeyTest() {
    ASSERT_TRUE(common_prefs_);
    auto key0 = common_prefs_->CreateSubKey({"ns1", "key"});
    // Corner case for "ns1".
    auto key0corner = common_prefs_->CreateSubKey({"ns11", "key"});
    auto key1A = common_prefs_->CreateSubKey({"ns1", "nsA", "keyA"});
    auto key1B = common_prefs_->CreateSubKey({"ns1", "nsA", "keyB"});
    auto key2 = common_prefs_->CreateSubKey({"ns1", "nsB", "key"});
    // Corner case for "ns1/nsB".
    auto key2corner = common_prefs_->CreateSubKey({"ns1", "nsB1", "key"});
    EXPECT_FALSE(common_prefs_->Exists(key0));
    EXPECT_FALSE(common_prefs_->Exists(key1A));
    EXPECT_FALSE(common_prefs_->Exists(key1B));
    EXPECT_FALSE(common_prefs_->Exists(key2));

    EXPECT_TRUE(common_prefs_->SetString(key0, ""));
    EXPECT_TRUE(common_prefs_->SetString(key0corner, ""));
    EXPECT_TRUE(common_prefs_->SetString(key1A, ""));
    EXPECT_TRUE(common_prefs_->SetString(key1B, ""));
    EXPECT_TRUE(common_prefs_->SetString(key2, ""));
    EXPECT_TRUE(common_prefs_->SetString(key2corner, ""));

    EXPECT_TRUE(common_prefs_->Exists(key0));
    EXPECT_TRUE(common_prefs_->Exists(key0corner));
    EXPECT_TRUE(common_prefs_->Exists(key1A));
    EXPECT_TRUE(common_prefs_->Exists(key1B));
    EXPECT_TRUE(common_prefs_->Exists(key2));
    EXPECT_TRUE(common_prefs_->Exists(key2corner));

    vector<string> keys2;
    EXPECT_TRUE(common_prefs_->GetSubKeys("ns1/nsB/", &keys2));
    EXPECT_THAT(keys2, ElementsAre(key2));
    for (const auto& key : keys2)
      EXPECT_TRUE(common_prefs_->Delete(key));
    EXPECT_TRUE(common_prefs_->Exists(key0));
    EXPECT_TRUE(common_prefs_->Exists(key0corner));
    EXPECT_TRUE(common_prefs_->Exists(key1A));
    EXPECT_TRUE(common_prefs_->Exists(key1B));
    EXPECT_FALSE(common_prefs_->Exists(key2));
    EXPECT_TRUE(common_prefs_->Exists(key2corner));

    vector<string> keys2corner;
    EXPECT_TRUE(common_prefs_->GetSubKeys("ns1/nsB", &keys2corner));
    EXPECT_THAT(keys2corner, ElementsAre(key2corner));
    for (const auto& key : keys2corner)
      EXPECT_TRUE(common_prefs_->Delete(key));
    EXPECT_FALSE(common_prefs_->Exists(key2corner));

    vector<string> keys1;
    EXPECT_TRUE(common_prefs_->GetSubKeys("ns1/nsA/", &keys1));
    EXPECT_THAT(keys1, UnorderedElementsAre(key1A, key1B));
    for (const auto& key : keys1)
      EXPECT_TRUE(common_prefs_->Delete(key));
    EXPECT_TRUE(common_prefs_->Exists(key0));
    EXPECT_TRUE(common_prefs_->Exists(key0corner));
    EXPECT_FALSE(common_prefs_->Exists(key1A));
    EXPECT_FALSE(common_prefs_->Exists(key1B));

    vector<string> keys0;
    EXPECT_TRUE(common_prefs_->GetSubKeys("ns1/", &keys0));
    EXPECT_THAT(keys0, ElementsAre(key0));
    for (const auto& key : keys0)
      EXPECT_TRUE(common_prefs_->Delete(key));
    EXPECT_FALSE(common_prefs_->Exists(key0));
    EXPECT_TRUE(common_prefs_->Exists(key0corner));

    vector<string> keys0corner;
    EXPECT_TRUE(common_prefs_->GetSubKeys("ns1", &keys0corner));
    EXPECT_THAT(keys0corner, ElementsAre(key0corner));
    for (const auto& key : keys0corner)
      EXPECT_TRUE(common_prefs_->Delete(key));
    EXPECT_FALSE(common_prefs_->Exists(key0corner));

    // Test sub directory namespace.
    const string kDlcPrefsSubDir = "foo-dir";
    key1A = common_prefs_->CreateSubKey({kDlcPrefsSubDir, "dlc1", "keyA"});
    EXPECT_TRUE(common_prefs_->SetString(key1A, "fp_1A"));
    key1B = common_prefs_->CreateSubKey({kDlcPrefsSubDir, "dlc1", "keyB"});
    EXPECT_TRUE(common_prefs_->SetString(key1B, "fp_1B"));
    auto key2A = common_prefs_->CreateSubKey({kDlcPrefsSubDir, "dlc2", "keyA"});
    EXPECT_TRUE(common_prefs_->SetString(key2A, "fp_A2"));

    vector<string> fpKeys;
    EXPECT_TRUE(common_prefs_->GetSubKeys(kDlcPrefsSubDir, &fpKeys));
    EXPECT_EQ(fpKeys.size(), 3UL);
    EXPECT_TRUE(common_prefs_->Delete(fpKeys[0]));
    EXPECT_TRUE(common_prefs_->Delete(fpKeys[1]));
    EXPECT_TRUE(common_prefs_->Delete(fpKeys[2]));
    EXPECT_FALSE(common_prefs_->Exists(key1A));
  }

  PrefsInterface* common_prefs_;
};

class PrefsTest : public BasePrefsTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    prefs_dir_ = temp_dir_.GetPath();
    ASSERT_TRUE(prefs_.Init(prefs_dir_));
    common_prefs_ = &prefs_;
  }

  bool SetValue(const string& key, const string& value) {
    return base::WriteFile(prefs_dir_.Append(key),
                           value.data(),
                           value.length()) == static_cast<int>(value.length());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath prefs_dir_;
  Prefs prefs_;
};

TEST(Prefs, Init) {
  Prefs prefs;
  const string ns1 = "ns1";
  const string ns2A = "ns2A";
  const string ns2B = "ns2B";
  const string sub_pref = "sp";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto ns1_path = temp_dir.GetPath().Append(ns1);
  auto ns2A_path = ns1_path.Append(ns2A);
  auto ns2B_path = ns1_path.Append(ns2B);
  auto sub_pref_path = ns2A_path.Append(sub_pref);

  EXPECT_TRUE(base::CreateDirectory(ns2B_path));
  EXPECT_TRUE(base::PathExists(ns2B_path));

  EXPECT_TRUE(base::CreateDirectory(sub_pref_path));
  EXPECT_TRUE(base::PathExists(sub_pref_path));

  EXPECT_TRUE(base::PathExists(ns1_path));
  ASSERT_TRUE(prefs.Init(temp_dir.GetPath()));
  EXPECT_FALSE(base::PathExists(ns1_path));
}

TEST_F(PrefsTest, GetFileNameForKey) {
  const char kAllvalidCharsKey[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-";
  base::FilePath path;
  EXPECT_TRUE(prefs_.file_storage_.GetFileNameForKey(kAllvalidCharsKey, &path));
  EXPECT_EQ(prefs_dir_.Append(kAllvalidCharsKey).value(), path.value());
}

TEST_F(PrefsTest, GetFileNameForKeyBadCharacter) {
  base::FilePath path;
  EXPECT_FALSE(prefs_.file_storage_.GetFileNameForKey("ABC abc", &path));
}

TEST_F(PrefsTest, GetFileNameForKeyEmpty) {
  base::FilePath path;
  EXPECT_FALSE(prefs_.file_storage_.GetFileNameForKey("", &path));
}

TEST_F(PrefsTest, CreateSubKey) {
  const string name_space = "ns";
  const string sub_pref1 = "sp1";
  const string sub_pref2 = "sp2";
  const string sub_key = "sk";

  EXPECT_EQ(PrefsInterface::CreateSubKey({name_space, sub_pref1, sub_key}),
            "ns/sp1/sk");
  EXPECT_EQ(PrefsInterface::CreateSubKey({name_space, sub_pref2, sub_key}),
            "ns/sp2/sk");
}

TEST_F(PrefsTest, GetString) {
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
  const char kValue[] = "some test value\non 2 lines";
  EXPECT_TRUE(prefs_.SetString(kKey, kValue));
  string value;
  EXPECT_TRUE(base::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ(kValue, value);
}

TEST_F(PrefsTest, SetStringBadKey) {
  const char kKeyWithDots[] = ".no-dots";
  EXPECT_FALSE(prefs_.SetString(kKeyWithDots, "some value"));
  EXPECT_FALSE(base::PathExists(prefs_dir_.Append(kKeyWithDots)));
}

TEST_F(PrefsTest, SetStringCreateDir) {
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
  EXPECT_FALSE(prefs_.SetString(kKey, "test value"));
}

TEST_F(PrefsTest, SetStringFileCreationFailure) {
  base::CreateDirectory(prefs_dir_.Append(kKey));
  EXPECT_FALSE(prefs_.SetString(kKey, "test value"));
  EXPECT_TRUE(base::DirectoryExists(prefs_dir_.Append(kKey)));
}

TEST_F(PrefsTest, GetInt64) {
  ASSERT_TRUE(SetValue(kKey, " \n 25 \t "));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(25, value);
}

TEST_F(PrefsTest, GetInt64BadValue) {
  ASSERT_TRUE(SetValue(kKey, "30a"));
  int64_t value;
  EXPECT_FALSE(prefs_.GetInt64(kKey, &value));
}

TEST_F(PrefsTest, GetInt64Max) {
  ASSERT_TRUE(SetValue(
      kKey,
      base::StringPrintf("%" PRIi64, std::numeric_limits<int64_t>::max())));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), value);
}

TEST_F(PrefsTest, GetInt64Min) {
  ASSERT_TRUE(SetValue(
      kKey,
      base::StringPrintf("%" PRIi64, std::numeric_limits<int64_t>::min())));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(std::numeric_limits<int64_t>::min(), value);
}

TEST_F(PrefsTest, GetInt64Negative) {
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
  EXPECT_TRUE(prefs_.SetInt64(kKey, -123));
  string value;
  EXPECT_TRUE(base::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ("-123", value);
}

TEST_F(PrefsTest, SetInt64BadKey) {
  const char kKeyWithSpaces[] = "s p a c e s";
  EXPECT_FALSE(prefs_.SetInt64(kKeyWithSpaces, 20));
  EXPECT_FALSE(base::PathExists(prefs_dir_.Append(kKeyWithSpaces)));
}

TEST_F(PrefsTest, SetInt64Max) {
  EXPECT_TRUE(prefs_.SetInt64(kKey, std::numeric_limits<int64_t>::max()));
  string value;
  EXPECT_TRUE(base::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ(base::StringPrintf("%" PRIi64, std::numeric_limits<int64_t>::max()),
            value);
}

TEST_F(PrefsTest, SetInt64Min) {
  EXPECT_TRUE(prefs_.SetInt64(kKey, std::numeric_limits<int64_t>::min()));
  string value;
  EXPECT_TRUE(base::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ(base::StringPrintf("%" PRIi64, std::numeric_limits<int64_t>::min()),
            value);
}

TEST_F(PrefsTest, GetBooleanFalse) {
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
  // test that the key doesn't exist before we set it.
  EXPECT_FALSE(prefs_.Exists(kKey));

  // test that the key exists after we set it.
  ASSERT_TRUE(prefs_.SetInt64(kKey, 8));
  EXPECT_TRUE(prefs_.Exists(kKey));
}

TEST_F(PrefsTest, DeleteWorks) {
  // test that it's alright to delete a non-existent key.
  EXPECT_TRUE(prefs_.Delete(kKey));

  // delete the key after we set it.
  ASSERT_TRUE(prefs_.SetInt64(kKey, 0));
  EXPECT_TRUE(prefs_.Delete(kKey));

  // make sure it doesn't exist anymore.
  EXPECT_FALSE(prefs_.Exists(kKey));
}

TEST_F(PrefsTest, SetDeleteSubKey) {
  const string name_space = "ns";
  const string sub_pref = "sp";
  const string sub_key1 = "sk1";
  const string sub_key2 = "sk2";
  auto key1 = prefs_.CreateSubKey({name_space, sub_pref, sub_key1});
  auto key2 = prefs_.CreateSubKey({name_space, sub_pref, sub_key2});
  base::FilePath sub_pref_path = prefs_dir_.Append(name_space).Append(sub_pref);

  ASSERT_TRUE(prefs_.SetInt64(key1, 0));
  ASSERT_TRUE(prefs_.SetInt64(key2, 0));
  EXPECT_TRUE(base::PathExists(sub_pref_path.Append(sub_key1)));
  EXPECT_TRUE(base::PathExists(sub_pref_path.Append(sub_key2)));

  ASSERT_TRUE(prefs_.Delete(key1));
  EXPECT_FALSE(base::PathExists(sub_pref_path.Append(sub_key1)));
  EXPECT_TRUE(base::PathExists(sub_pref_path.Append(sub_key2)));
  ASSERT_TRUE(prefs_.Delete(key2));
  EXPECT_FALSE(base::PathExists(sub_pref_path.Append(sub_key2)));
  prefs_.Init(prefs_dir_);
  EXPECT_FALSE(base::PathExists(prefs_dir_.Append(name_space)));
}

TEST_F(PrefsTest, DeletePrefs) {
  const string kPrefsSubDir = "foo-dir";
  const string kFpKey = "kPrefFp";
  const string kNotFpKey = "NotkPrefFp";
  const string kOtherKey = "kPrefNotFp";

  EXPECT_TRUE(prefs_.SetString(kFpKey, "3.000"));
  EXPECT_TRUE(prefs_.SetString(kOtherKey, "not_fp_val"));

  auto key1_fp = prefs_.CreateSubKey({kPrefsSubDir, "id-1", kFpKey});
  EXPECT_TRUE(prefs_.SetString(key1_fp, "3.7"));
  auto key_not_fp = prefs_.CreateSubKey({kPrefsSubDir, "id-1", kOtherKey});
  EXPECT_TRUE(prefs_.SetString(key_not_fp, "not_fp_val"));
  auto key2_fp = prefs_.CreateSubKey({kPrefsSubDir, "id-2", kFpKey});
  EXPECT_TRUE(prefs_.SetString(key2_fp, "3.9"));
  auto key3_fp = prefs_.CreateSubKey({kPrefsSubDir, "id-3", kFpKey});
  EXPECT_TRUE(prefs_.SetString(key3_fp, "3.45"));

  // Pref key does not match full subkey at end, should not delete.
  auto key_middle_fp = prefs_.CreateSubKey({kPrefsSubDir, kFpKey, kOtherKey});
  EXPECT_TRUE(prefs_.SetString(key_middle_fp, "not_fp_val"));
  auto key_end_not_fp = prefs_.CreateSubKey({kPrefsSubDir, "id-1", kNotFpKey});
  EXPECT_TRUE(prefs_.SetString(key_end_not_fp, "not_fp_val"));

  // Delete key in platform and one namespace.
  prefs_.Delete(kFpKey, {kPrefsSubDir});

  EXPECT_FALSE(prefs_.Exists(kFpKey));
  EXPECT_FALSE(prefs_.Exists(key1_fp));
  EXPECT_FALSE(prefs_.Exists(key2_fp));
  EXPECT_FALSE(prefs_.Exists(key3_fp));

  // Check other keys are not deleted.
  EXPECT_TRUE(prefs_.Exists(kOtherKey));
  EXPECT_TRUE(prefs_.Exists(key_not_fp));
  EXPECT_TRUE(prefs_.Exists(key_middle_fp));
  EXPECT_TRUE(prefs_.Exists(key_end_not_fp));
}

TEST_F(PrefsTest, DeleteMultipleNamespaces) {
  const string kFirstSubDir = "foo-dir";
  const string kSecondarySubDir = "bar-dir";
  const string kTertiarySubDir = "ter-dir";
  const string kFpKey = "kPrefFp";

  EXPECT_TRUE(prefs_.SetString(kFpKey, "3.000"));
  // Set pref key in different namespaces.
  auto key1_fp = prefs_.CreateSubKey({kFirstSubDir, "id-1", kFpKey});
  EXPECT_TRUE(prefs_.SetString(key1_fp, "3.7"));
  auto key2_fp = prefs_.CreateSubKey({kSecondarySubDir, "id-3", kFpKey});
  EXPECT_TRUE(prefs_.SetString(key2_fp, "7.45"));
  auto key3_fp = prefs_.CreateSubKey({kTertiarySubDir, "id-3", kFpKey});
  EXPECT_TRUE(prefs_.SetString(key3_fp, "7.45"));

  // Delete key in platform and given namespaces.
  prefs_.Delete(kFpKey, {kFirstSubDir, kSecondarySubDir});

  EXPECT_FALSE(prefs_.Exists(kFpKey));
  EXPECT_FALSE(prefs_.Exists(key1_fp));
  EXPECT_FALSE(prefs_.Exists(key2_fp));

  // Tertiary namespace not given to delete. Key should still exist.
  EXPECT_TRUE(prefs_.Exists(key3_fp));
}

class MockPrefsObserver : public PrefsInterface::ObserverInterface {
 public:
  MOCK_METHOD1(OnPrefSet, void(const string&));
  MOCK_METHOD1(OnPrefDeleted, void(const string& key));
};

TEST_F(PrefsTest, ObserversCalled) {
  MockPrefsObserver mock_obserser;
  prefs_.AddObserver(kKey, &mock_obserser);

  EXPECT_CALL(mock_obserser, OnPrefSet(Eq(kKey)));
  EXPECT_CALL(mock_obserser, OnPrefDeleted(_)).Times(0);
  prefs_.SetString(kKey, "value");
  testing::Mock::VerifyAndClearExpectations(&mock_obserser);

  EXPECT_CALL(mock_obserser, OnPrefSet(_)).Times(0);
  EXPECT_CALL(mock_obserser, OnPrefDeleted(Eq(kKey)));
  prefs_.Delete(kKey);
  testing::Mock::VerifyAndClearExpectations(&mock_obserser);

  auto key1 = prefs_.CreateSubKey({"ns", "sp1", "key1"});
  prefs_.AddObserver(key1, &mock_obserser);

  EXPECT_CALL(mock_obserser, OnPrefSet(key1));
  EXPECT_CALL(mock_obserser, OnPrefDeleted(_)).Times(0);
  prefs_.SetString(key1, "value");
  testing::Mock::VerifyAndClearExpectations(&mock_obserser);

  EXPECT_CALL(mock_obserser, OnPrefSet(_)).Times(0);
  EXPECT_CALL(mock_obserser, OnPrefDeleted(Eq(key1)));
  prefs_.Delete(key1);
  testing::Mock::VerifyAndClearExpectations(&mock_obserser);

  prefs_.RemoveObserver(kKey, &mock_obserser);
}

TEST_F(PrefsTest, OnlyCalledOnObservedKeys) {
  MockPrefsObserver mock_obserser;
  const char kUnusedKey[] = "unused-key";
  prefs_.AddObserver(kUnusedKey, &mock_obserser);

  EXPECT_CALL(mock_obserser, OnPrefSet(_)).Times(0);
  EXPECT_CALL(mock_obserser, OnPrefDeleted(_)).Times(0);
  prefs_.SetString(kKey, "value");
  prefs_.Delete(kKey);

  prefs_.RemoveObserver(kUnusedKey, &mock_obserser);
}

TEST_F(PrefsTest, RemovedObserversNotCalled) {
  MockPrefsObserver mock_obserser_a, mock_obserser_b;
  prefs_.AddObserver(kKey, &mock_obserser_a);
  prefs_.AddObserver(kKey, &mock_obserser_b);
  EXPECT_CALL(mock_obserser_a, OnPrefSet(_)).Times(2);
  EXPECT_CALL(mock_obserser_b, OnPrefSet(_)).Times(1);
  EXPECT_TRUE(prefs_.SetString(kKey, "value"));
  prefs_.RemoveObserver(kKey, &mock_obserser_b);
  EXPECT_TRUE(prefs_.SetString(kKey, "other value"));
  prefs_.RemoveObserver(kKey, &mock_obserser_a);
  EXPECT_TRUE(prefs_.SetString(kKey, "yet another value"));
}

TEST_F(PrefsTest, UnsuccessfulCallsNotObserved) {
  MockPrefsObserver mock_obserser;
  const char kInvalidKey[] = "no spaces or .";
  prefs_.AddObserver(kInvalidKey, &mock_obserser);

  EXPECT_CALL(mock_obserser, OnPrefSet(_)).Times(0);
  EXPECT_CALL(mock_obserser, OnPrefDeleted(_)).Times(0);
  EXPECT_FALSE(prefs_.SetString(kInvalidKey, "value"));
  EXPECT_FALSE(prefs_.Delete(kInvalidKey));

  prefs_.RemoveObserver(kInvalidKey, &mock_obserser);
}

TEST_F(PrefsTest, MultiNamespaceKeyTest) {
  MultiNamespaceKeyTest();
}

class MemoryPrefsTest : public BasePrefsTest {
 protected:
  void SetUp() override { common_prefs_ = &prefs_; }

  MemoryPrefs prefs_;
};

TEST_F(MemoryPrefsTest, BasicTest) {
  EXPECT_FALSE(prefs_.Exists(kKey));
  int64_t value = 0;
  EXPECT_FALSE(prefs_.GetInt64(kKey, &value));

  EXPECT_TRUE(prefs_.SetInt64(kKey, 1234));
  EXPECT_TRUE(prefs_.Exists(kKey));
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(1234, value);

  EXPECT_TRUE(prefs_.Delete(kKey));
  EXPECT_FALSE(prefs_.Exists(kKey));
  EXPECT_TRUE(prefs_.Delete(kKey));

  auto key = prefs_.CreateSubKey({"ns", "sp", "sk"});
  ASSERT_TRUE(prefs_.SetInt64(key, 0));
  EXPECT_TRUE(prefs_.Exists(key));
  EXPECT_TRUE(prefs_.Delete(kKey));
}

TEST_F(MemoryPrefsTest, MultiNamespaceKeyTest) {
  MultiNamespaceKeyTest();
}

}  // namespace chromeos_update_engine
