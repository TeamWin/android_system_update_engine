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

#include "update_engine/cros/excluder_chromeos.h"

#include <memory>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_piece.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>

#include "update_engine/common/constants.h"
#include "update_engine/common/system_state.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

std::unique_ptr<ExcluderInterface> CreateExcluder() {
  return std::make_unique<ExcluderChromeOS>();
}

bool ExcluderChromeOS::Exclude(const string& name) {
  auto* prefs = SystemState::Get()->prefs();
  auto key = prefs->CreateSubKey({kExclusionPrefsSubDir, name});
  return prefs->SetString(key, "");
}

bool ExcluderChromeOS::IsExcluded(const string& name) {
  auto* prefs = SystemState::Get()->prefs();
  auto key = prefs->CreateSubKey({kExclusionPrefsSubDir, name});
  return prefs->Exists(key);
}

bool ExcluderChromeOS::Reset() {
  auto* prefs = SystemState::Get()->prefs();
  bool ret = true;
  vector<string> keys;
  if (!prefs->GetSubKeys(kExclusionPrefsSubDir, &keys))
    return false;
  for (const auto& key : keys)
    if (!(ret &= prefs->Delete(key)))
      LOG(ERROR) << "Failed to delete exclusion pref for " << key;
  return ret;
}

}  // namespace chromeos_update_engine
