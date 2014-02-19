// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/hwid_override.h"

#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>

#include "update_engine/simple_key_value_store.h"

using std::map;
using std::string;

namespace chromeos_update_engine {

const char HwidOverride::kHwidOverrideKey[] = "HWID_OVERRIDE";

HwidOverride::HwidOverride() {}

HwidOverride::~HwidOverride() {}

std::string HwidOverride::Read(const base::FilePath& root) {
  base::FilePath kFile(root.value() + "/etc/lsb-release");
  string file_data;
  map<string, string> data;
  if (file_util::ReadFileToString(kFile, &file_data)) {
    data = simple_key_value_store::ParseString(file_data);
  }
  return data[kHwidOverrideKey];
}

}  // namespace chromeos_update_engine
