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

#include "update_engine/payload_generator/payload_properties.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <base/json/json_writer.h>
#include <base/strings/string_util.h>
#include <base/values.h>
#include <brillo/data_encoding.h>

#include "update_engine/common/constants.h"
#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/payload_metadata.h"
#include "update_engine/update_metadata.pb.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
// These ones are needed by the GoldenEye.
const char kPayloadPropertyJsonVersion[] = "version";
const char kPayloadPropertyJsonPayloadHash[] = "sha256_hex";
const char kPayloadPropertyJsonMetadataSize[] = "metadata_size";
const char kPayloadPropertyJsonMetadataSignature[] = "metadata_signature";

// These are needed by the Nebraska and devserver.
const char kPayloadPropertyJsonPayloadSize[] = "size";
const char kPayloadPropertyJsonIsDelta[] = "is_delta";
}  // namespace

PayloadProperties::PayloadProperties(const string& payload_path)
    : payload_path_(payload_path) {}

bool PayloadProperties::GetPropertiesAsJson(string* json_str) {
  TEST_AND_RETURN_FALSE(LoadFromPayload());

  base::DictionaryValue properties;
  properties.SetInteger(kPayloadPropertyJsonVersion, version_);
  properties.SetInteger(kPayloadPropertyJsonMetadataSize, metadata_size_);
  properties.SetString(kPayloadPropertyJsonMetadataSignature,
                       metadata_signatures_);
  properties.SetInteger(kPayloadPropertyJsonPayloadSize, payload_size_);
  properties.SetString(kPayloadPropertyJsonPayloadHash, payload_hash_);
  properties.SetBoolean(kPayloadPropertyJsonIsDelta, is_delta_);

  return base::JSONWriter::Write(properties, json_str);
}

bool PayloadProperties::GetPropertiesAsKeyValue(string* key_value_str) {
  TEST_AND_RETURN_FALSE(LoadFromPayload());

  brillo::KeyValueStore properties;
  properties.SetString(kPayloadPropertyFileSize, std::to_string(payload_size_));
  properties.SetString(kPayloadPropertyMetadataSize,
                       std::to_string(metadata_size_));
  properties.SetString(kPayloadPropertyFileHash, payload_hash_);
  properties.SetString(kPayloadPropertyMetadataHash, metadata_hash_);

  *key_value_str = properties.SaveToString();
  return true;
}

bool PayloadProperties::LoadFromPayload() {
  PayloadMetadata payload_metadata;
  DeltaArchiveManifest manifest;
  Signatures metadata_signatures;
  TEST_AND_RETURN_FALSE(payload_metadata.ParsePayloadFile(
      payload_path_, &manifest, &metadata_signatures));

  metadata_size_ = payload_metadata.GetMetadataSize();
  payload_size_ = utils::FileSize(payload_path_);

  brillo::Blob metadata_hash;
  TEST_AND_RETURN_FALSE(HashCalculator::RawHashOfFile(
                            payload_path_, metadata_size_, &metadata_hash) ==
                        static_cast<off_t>(metadata_size_));
  metadata_hash_ = brillo::data_encoding::Base64Encode(metadata_hash);

  brillo::Blob payload_hash;
  TEST_AND_RETURN_FALSE(HashCalculator::RawHashOfFile(
                            payload_path_, payload_size_, &payload_hash) ==
                        static_cast<off_t>(payload_size_));
  payload_hash_ = brillo::data_encoding::Base64Encode(payload_hash);

  if (payload_metadata.GetMetadataSignatureSize() > 0) {
    TEST_AND_RETURN_FALSE(metadata_signatures.signatures_size() > 0);
    vector<string> base64_signatures;
    for (const auto& sig : metadata_signatures.signatures()) {
      base64_signatures.push_back(
          brillo::data_encoding::Base64Encode(sig.data()));
    }
    metadata_signatures_ = base::JoinString(base64_signatures, ":");
  }

  is_delta_ = std::any_of(manifest.partitions().begin(),
                          manifest.partitions().end(),
                          [](const PartitionUpdate& part) {
                            return part.has_old_partition_info();
                          });
  return true;
}

}  // namespace chromeos_update_engine
