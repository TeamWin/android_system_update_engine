// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/simple_key_value_store.h"

#include <map>
#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

#include "update_engine/utils.h"

using std::map;
using std::string;
using std::vector;

namespace chromeos_update_engine {

bool KeyValueStore::Load(const string& filename) {
  string file_data;
  if (!base::ReadFileToString(base::FilePath(filename), &file_data))
    return false;

  // Split along '\n', then along '='
  vector<string> lines;
  base::SplitStringDontTrim(file_data, '\n', &lines);
  for (auto& it : lines) {
    if (it.empty() || it[0] == '#')
      continue;
    string::size_type pos = it.find('=');
    if (pos == string::npos)
      continue;
    store_[it.substr(0, pos)] = it.substr(pos + 1);
  }
  return true;
}

bool KeyValueStore::Save(const string& filename) const {
  string data;
  for (auto& it : store_)
    data += it.first + "=" + it.second + "\n";

  return utils::WriteFile(filename.c_str(), data.c_str(), data.size());
}

bool KeyValueStore::GetString(const string& key, string* value) const {
  auto it = store_.find(key);
  if (it == store_.end())
    return false;
  *value = it->second;
  return true;
}

void KeyValueStore::SetString(const string& key, const string& value) {
  store_[key] = value;
}

bool KeyValueStore::GetBoolean(const string& key, bool* value) const {
  auto it = store_.find(key);
  if (it == store_.end())
    return false;
  if (it->second == "true") {
    *value = true;
    return true;
  } else if (it-> second == "false") {
    *value = false;
    return true;
  }

  return false;
}

void KeyValueStore::SetBoolean(const string& key, bool value) {
  store_[key] = value ? "true" : "false";
}

}  // namespace chromeos_update_engine
