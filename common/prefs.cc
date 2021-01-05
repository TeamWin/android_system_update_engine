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

#include <algorithm>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

#include "update_engine/common/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

void DeleteEmptyDirectories(const base::FilePath& path) {
  base::FileEnumerator path_enum(
      path, false /* recursive */, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath dir_path = path_enum.Next(); !dir_path.empty();
       dir_path = path_enum.Next()) {
    DeleteEmptyDirectories(dir_path);
    if (base::IsDirectoryEmpty(dir_path))
#if BASE_VER < 800000
      base::DeleteFile(dir_path, false);
#else
      base::DeleteFile(dir_path);
#endif
  }
}

}  // namespace

bool PrefsBase::GetString(const string& key, string* value) const {
  return storage_->GetKey(key, value);
}

bool PrefsBase::SetString(const string& key, std::string_view value) {
  TEST_AND_RETURN_FALSE(storage_->SetKey(key, value));
  const auto observers_for_key = observers_.find(key);
  if (observers_for_key != observers_.end()) {
    std::vector<ObserverInterface*> copy_observers(observers_for_key->second);
    for (ObserverInterface* observer : copy_observers)
      observer->OnPrefSet(key);
  }
  return true;
}

bool PrefsBase::GetInt64(const string& key, int64_t* value) const {
  string str_value;
  if (!GetString(key, &str_value))
    return false;
  base::TrimWhitespaceASCII(str_value, base::TRIM_ALL, &str_value);
  TEST_AND_RETURN_FALSE(base::StringToInt64(str_value, value));
  return true;
}

bool PrefsBase::SetInt64(const string& key, const int64_t value) {
  return SetString(key, base::NumberToString(value));
}

bool PrefsBase::GetBoolean(const string& key, bool* value) const {
  string str_value;
  if (!GetString(key, &str_value))
    return false;
  base::TrimWhitespaceASCII(str_value, base::TRIM_ALL, &str_value);
  if (str_value == "false") {
    *value = false;
    return true;
  }
  if (str_value == "true") {
    *value = true;
    return true;
  }
  return false;
}

bool PrefsBase::SetBoolean(const string& key, const bool value) {
  return SetString(key, value ? "true" : "false");
}

bool PrefsBase::Exists(const string& key) const {
  return storage_->KeyExists(key);
}

bool PrefsBase::Delete(const string& key) {
  TEST_AND_RETURN_FALSE(storage_->DeleteKey(key));
  const auto observers_for_key = observers_.find(key);
  if (observers_for_key != observers_.end()) {
    std::vector<ObserverInterface*> copy_observers(observers_for_key->second);
    for (ObserverInterface* observer : copy_observers)
      observer->OnPrefDeleted(key);
  }
  return true;
}

bool PrefsBase::Delete(const string& pref_key, const vector<string>& nss) {
  // Delete pref key for platform.
  bool success = Delete(pref_key);
  // Delete pref key in each namespace.
  for (const auto& ns : nss) {
    vector<string> namespace_keys;
    success = GetSubKeys(ns, &namespace_keys) && success;
    for (const auto& key : namespace_keys) {
      auto last_key_seperator = key.find_last_of(kKeySeparator);
      if (last_key_seperator != string::npos &&
          pref_key == key.substr(last_key_seperator + 1)) {
        success = Delete(key) && success;
      }
    }
  }
  return success;
}

bool PrefsBase::GetSubKeys(const string& ns, vector<string>* keys) const {
  return storage_->GetSubKeys(ns, keys);
}

void PrefsBase::AddObserver(const string& key, ObserverInterface* observer) {
  observers_[key].push_back(observer);
}

void PrefsBase::RemoveObserver(const string& key, ObserverInterface* observer) {
  std::vector<ObserverInterface*>& observers_for_key = observers_[key];
  auto observer_it =
      std::find(observers_for_key.begin(), observers_for_key.end(), observer);
  if (observer_it != observers_for_key.end())
    observers_for_key.erase(observer_it);
}

string PrefsInterface::CreateSubKey(const vector<string>& ns_and_key) {
  return base::JoinString(ns_and_key, string(1, kKeySeparator));
}

// Prefs

bool Prefs::Init(const base::FilePath& prefs_dir) {
  return file_storage_.Init(prefs_dir);
}

bool Prefs::FileStorage::Init(const base::FilePath& prefs_dir) {
  prefs_dir_ = prefs_dir;
  // Delete empty directories. Ignore errors when deleting empty directories.
  DeleteEmptyDirectories(prefs_dir_);
  return true;
}

bool Prefs::FileStorage::GetKey(const string& key, string* value) const {
  base::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  if (!base::ReadFileToString(filename, value)) {
    return false;
  }
  return true;
}

bool Prefs::FileStorage::GetSubKeys(const string& ns,
                                    vector<string>* keys) const {
  base::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(ns, &filename));
  base::FileEnumerator namespace_enum(
      prefs_dir_, true, base::FileEnumerator::FILES);
  for (base::FilePath f = namespace_enum.Next(); !f.empty();
       f = namespace_enum.Next()) {
    auto filename_str = filename.value();
    if (f.value().compare(0, filename_str.length(), filename_str) == 0) {
      // Only return the key portion excluding the |prefs_dir_| with slash.
      keys->push_back(f.value().substr(
          prefs_dir_.AsEndingWithSeparator().value().length()));
    }
  }
  return true;
}

bool Prefs::FileStorage::SetKey(const string& key, std::string_view value) {
  base::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  if (!base::DirectoryExists(filename.DirName())) {
    // Only attempt to create the directory if it doesn't exist to avoid calls
    // to parent directories where we might not have permission to write to.
    TEST_AND_RETURN_FALSE(base::CreateDirectory(filename.DirName()));
  }
  TEST_AND_RETURN_FALSE(base::WriteFile(filename, value.data(), value.size()) ==
                        static_cast<int>(value.size()));
  return true;
}

bool Prefs::FileStorage::KeyExists(const string& key) const {
  base::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  return base::PathExists(filename);
}

bool Prefs::FileStorage::DeleteKey(const string& key) {
  base::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
#if BASE_VER < 800000
  TEST_AND_RETURN_FALSE(base::DeleteFile(filename, false));
#else
  TEST_AND_RETURN_FALSE(base::DeleteFile(filename));
#endif
  return true;
}

bool Prefs::FileStorage::GetFileNameForKey(const string& key,
                                           base::FilePath* filename) const {
  // Allows only non-empty keys containing [A-Za-z0-9_-/].
  TEST_AND_RETURN_FALSE(!key.empty());
  for (char c : key)
    TEST_AND_RETURN_FALSE(base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) ||
                          c == '_' || c == '-' || c == kKeySeparator);
  *filename = prefs_dir_.Append(key);
  return true;
}

// MemoryPrefs

bool MemoryPrefs::MemoryStorage::GetKey(const string& key,
                                        string* value) const {
  auto it = values_.find(key);
  if (it == values_.end())
    return false;
  *value = it->second;
  return true;
}

bool MemoryPrefs::MemoryStorage::GetSubKeys(const string& ns,
                                            vector<string>* keys) const {
  using value_type = decltype(values_)::value_type;
  using key_type = decltype(values_)::key_type;
  auto lower_comp = [](const value_type& pr, const key_type& ns) {
    return pr.first.substr(0, ns.length()) < ns;
  };
  auto upper_comp = [](const key_type& ns, const value_type& pr) {
    return ns < pr.first.substr(0, ns.length());
  };
  auto lower_it =
      std::lower_bound(begin(values_), end(values_), ns, lower_comp);
  auto upper_it = std::upper_bound(lower_it, end(values_), ns, upper_comp);
  while (lower_it != upper_it)
    keys->push_back((lower_it++)->first);
  return true;
}

bool MemoryPrefs::MemoryStorage::SetKey(const string& key,
                                        std::string_view value) {
  values_[key] = value;
  return true;
}

bool MemoryPrefs::MemoryStorage::KeyExists(const string& key) const {
  return values_.find(key) != values_.end();
}

bool MemoryPrefs::MemoryStorage::DeleteKey(const string& key) {
  auto it = values_.find(key);
  if (it != values_.end())
    values_.erase(it);
  return true;
}

}  // namespace chromeos_update_engine
