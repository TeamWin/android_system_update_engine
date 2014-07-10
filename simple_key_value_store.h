// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These functions can parse a blob of data that's formatted as a simple
// key value store. Each key/value pair is stored on its own line and
// separated by the first '=' on the line.

#ifndef UPDATE_ENGINE_SIMPLE_KEY_VALUE_STORE_H_
#define UPDATE_ENGINE_SIMPLE_KEY_VALUE_STORE_H_

#include <map>
#include <string>

namespace chromeos_update_engine {

class KeyValueStore {
 public:
  // Creates an empty KeyValueStore.
  KeyValueStore() {}

  // Loads the key=value pairs from the given filename. Lines starting with
  // '#' and empty lines are ignored. Adds all the readed key=values to the
  // store, overriding those already defined but persisting the ones that
  // aren't present on the passed file.
  // Returns whether reading the file succeeded.
  bool Load(const std::string& filename);

  // Saves the current store to the given |filename| file. Returns whether the
  // file creation succeeded.
  bool Save(const std::string& filename) const;

  // Getter for the given key. Returns whether the key was found on the store.
  bool GetString(const std::string& key, std::string* value) const;

  // Setter for the given key. It overrides the key if already exists.
  void SetString(const std::string& key, const std::string& value);

  // Boolean getter. Returns whether the key was found on the store and if it
  // has a valid value ("true" or "false").
  bool GetBoolean(const std::string& key, bool* value) const;

  // Boolean setter. Sets the value as "true" or "false".
  void SetBoolean(const std::string& key, bool value);

 private:
  // The map storing all the key-value pairs.
  std::map<std::string, std::string> store_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_SIMPLE_KEY_VALUE_STORE_H_
