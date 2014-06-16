// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PREFS_MOCK_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PREFS_MOCK_H_

#include <string>

#include "gmock/gmock.h"
#include "update_engine/constants.h"
#include "update_engine/prefs_interface.h"

namespace chromeos_update_engine {

class PrefsMock : public PrefsInterface {
 public:
  MOCK_METHOD2(GetString, bool(const std::string& key, std::string* value));
  MOCK_METHOD2(SetString, bool(const std::string& key,
                               const std::string& value));
  MOCK_METHOD2(GetInt64, bool(const std::string& key, int64_t* value));
  MOCK_METHOD2(SetInt64, bool(const std::string& key, const int64_t value));

  MOCK_METHOD2(GetBoolean, bool(const std::string& key, bool* value));
  MOCK_METHOD2(SetBoolean, bool(const std::string& key, const bool value));

  MOCK_METHOD1(Exists, bool(const std::string& key));
  MOCK_METHOD1(Delete, bool(const std::string& key));
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PREFS_MOCK_H_
