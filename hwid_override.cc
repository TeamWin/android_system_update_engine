// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/hwid_override.h"

#include <map>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>

#include "update_engine/simple_key_value_store.h"

using std::map;
using std::string;

namespace chromeos_update_engine {

const char HwidOverride::kHwidOverrideKey[] = "HWID_OVERRIDE";

HwidOverride::HwidOverride() {}

HwidOverride::~HwidOverride() {}

std::string HwidOverride::Read(const base::FilePath& root) {
  KeyValueStore lsb_release;
  lsb_release.Load(root.value() + "/etc/lsb-release");
  string result;
  if (lsb_release.GetString(kHwidOverrideKey, &result))
    return result;
  return "";
}

}  // namespace chromeos_update_engine
