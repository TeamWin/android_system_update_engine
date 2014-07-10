// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_FAKE_PREFS_H_
#define UPDATE_ENGINE_FAKE_PREFS_H_

#include <map>
#include <string>

#include <base/basictypes.h>

#include "update_engine/prefs_interface.h"

namespace chromeos_update_engine {

// Implements a fake preference store by storing the value associated with
// a key in a std::map, suitable for testing. It doesn't allow to set a value on
// a key with a different type than the previously set type. This enforces the
// type of a given key to be fixed. Also the class checks that the Get*()
// methods aren't called on a key set with a different type.

class FakePrefs : public PrefsInterface {
 public:
  FakePrefs() {}

  // PrefsInterface methods.
  bool GetString(const std::string& key, std::string* value) override;
  bool SetString(const std::string& key, const std::string& value) override;
  bool GetInt64(const std::string& key, int64_t* value) override;
  bool SetInt64(const std::string& key, const int64_t value) override;
  bool GetBoolean(const std::string& key, bool* value) override;
  bool SetBoolean(const std::string& key, const bool value) override;

  bool Exists(const std::string& key) override;
  bool Delete(const std::string& key) override;

 private:
  enum class PrefType {
    kString,
    kInt64,
    kBool,
  };
  struct PrefValue {
    std::string as_str;
    int64_t as_int64;
    bool as_bool;
  };

  struct PrefTypeValue {
    PrefType type;
    PrefValue value;
  };

  // Class to store compile-time type dependant constants.
  template<typename T>
  class PrefConsts {
   public:
    // The PrefType associated with T.
    static FakePrefs::PrefType const type;

    // The data member pointer to PrefValue associated with T.
    static T FakePrefs::PrefValue::* const member;
  };

  // Returns a string representation of the PrefType useful for logging.
  static std::string GetTypeName(PrefType type);

  // Checks that the |key| is either not present or has the given |type|.
  void CheckKeyType(const std::string& key, PrefType type) const;

  // Helper function to set a value of the passed |key|. It sets the type based
  // on the template parameter T.
  template<typename T>
  void SetValue(const std::string& key, const T& value);

  // Helper function to get a value from the map checking for invalid calls.
  // The function fails the test if you attempt to read a value  defined as a
  // different type. Returns whether the get succeeded.
  template<typename T>
  bool GetValue(const std::string& key, T* value) const;

  // Container for all the key/value pairs.
  std::map<std::string, PrefTypeValue> values_;

  DISALLOW_COPY_AND_ASSIGN(FakePrefs);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_FAKE_PREFS_H_
