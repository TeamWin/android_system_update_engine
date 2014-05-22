// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/prefs.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

bool Prefs::Init(const base::FilePath& prefs_dir) {
  prefs_dir_ = prefs_dir;
  return true;
}

bool Prefs::GetString(const string& key, string* value) {
  base::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  if (!base::ReadFileToString(filename, value)) {
    LOG(INFO) << key << " not present in " << prefs_dir_.value();
    return false;
  }
  return true;
}

bool Prefs::SetString(const std::string& key, const std::string& value) {
  base::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  TEST_AND_RETURN_FALSE(base::CreateDirectory(filename.DirName()));
  TEST_AND_RETURN_FALSE(base::WriteFile(filename, value.data(), value.size()) ==
                        static_cast<int>(value.size()));
  return true;
}

bool Prefs::GetInt64(const string& key, int64_t* value) {
  string str_value;
  if (!GetString(key, &str_value))
    return false;
  base::TrimWhitespaceASCII(str_value, base::TRIM_ALL, &str_value);
  TEST_AND_RETURN_FALSE(base::StringToInt64(str_value, value));
  return true;
}

bool Prefs::SetInt64(const string& key, const int64_t value) {
  return SetString(key, base::Int64ToString(value));
}

bool Prefs::GetBoolean(const std::string& key, bool* value) {
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

bool Prefs::SetBoolean(const std::string& key, const bool value) {
  return SetString(key, value ? "true" : "false");
}

bool Prefs::Exists(const string& key) {
  base::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  return base::PathExists(filename);
}

bool Prefs::Delete(const string& key) {
  base::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  return base::DeleteFile(filename, false);
}

bool Prefs::GetFileNameForKey(const std::string& key,
                              base::FilePath* filename) {
  // Allows only non-empty keys containing [A-Za-z0-9_-].
  TEST_AND_RETURN_FALSE(!key.empty());
  for (size_t i = 0; i < key.size(); ++i) {
    char c = key.at(i);
    TEST_AND_RETURN_FALSE(IsAsciiAlpha(c) || IsAsciiDigit(c) ||
                          c == '_' || c == '-');
  }
  *filename = prefs_dir_.Append(key);
  return true;
}

}  // namespace chromeos_update_engine
