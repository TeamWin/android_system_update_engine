//
// Copyright 2019 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_PAYLOAD_PROPERTIES_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_PAYLOAD_PROPERTIES_H_

#include <string>

#include <brillo/key_value_store.h>
#include <brillo/secure_blob.h>

namespace chromeos_update_engine {

// A class for extracting information about a payload from the payload file
// itself. Currently the metadata can be exported as a json file or a key/value
// properties file. But more can be added if required.
class PayloadProperties {
 public:
  explicit PayloadProperties(const std::string& payload_path);
  ~PayloadProperties() = default;

  // Get the properties in a json format. The json file will be used in
  // autotests, cros flash, etc. Mainly in Chrome OS.
  bool GetPropertiesAsJson(std::string* json_str);

  // Get the properties of the payload as a key/value store. This is mainly used
  // in Android.
  bool GetPropertiesAsKeyValue(std::string* key_value_str);

 private:
  // Does the main job of reading the payload and extracting information from
  // it.
  bool LoadFromPayload();

  // The path to the payload file.
  std::string payload_path_;

  // The version of the metadata json format. If the output json file changes
  // format, this needs to be increased.
  int version_{2};

  size_t metadata_size_;
  std::string metadata_hash_;
  std::string metadata_signatures_;

  size_t payload_size_;
  std::string payload_hash_;

  // Whether the payload is a delta (true) or full (false).
  bool is_delta_;

  DISALLOW_COPY_AND_ASSIGN(PayloadProperties);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_PAYLOAD_PROPERTIES_H_
