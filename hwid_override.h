// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_HWID_OVERRIDE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_HWID_OVERRIDE_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>

namespace chromeos_update_engine {

// Class that allows HWID to be read from <root>/etc/lsb-release.
class HwidOverride {
 public:
  HwidOverride();
  ~HwidOverride();

  // Read HWID from an /etc/lsb-release file under given root.
  // An empty string is returned if there is any error.
  static std::string Read(const base::FilePath& root);

  static const char kHwidOverrideKey[];
 private:
  DISALLOW_COPY_AND_ASSIGN(HwidOverride);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_HWID_OVERRIDE_H_
