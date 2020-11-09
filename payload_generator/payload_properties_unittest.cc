//
// Copyright (C) 2019 The Android Open Source Project
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

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/rand_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/data_encoding.h>

#include <gtest/gtest.h>

#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/install_plan.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/delta_diff_utils.h"
#include "update_engine/payload_generator/full_update_generator.h"
#include "update_engine/payload_generator/operations_generator.h"
#include "update_engine/payload_generator/payload_file.h"
#include "update_engine/payload_generator/payload_generation_config.h"

using std::string;
using std::unique_ptr;
using std::vector;

namespace chromeos_update_engine {

// TODO(kimjae): current implementation is very specific to a static way of
// producing a deterministic test. It would definitely be beneficial to
// extend the |PayloadPropertiesTest::SetUp()| into a generic helper or
// seperate class that can handle creation of different |PayloadFile|s.
class PayloadPropertiesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    PayloadGenerationConfig config;
    config.version.major = kBrilloMajorPayloadVersion;
    config.version.minor = kSourceMinorPayloadVersion;
    PayloadFile payload;
    EXPECT_TRUE(payload.Init(config));

    const auto SetupPartitionConfig =
        [](PartitionConfig* config, const string& path, size_t size) {
          config->path = path;
          config->size = size;
        };
    const auto WriteZerosToFile = [](const char path[], size_t size) {
      string zeros(size, '\0');
      EXPECT_TRUE(utils::WriteFile(path, zeros.c_str(), zeros.size()));
    };
    ScopedTempFile old_part_file("old_part.XXXXXX");
    ScopedTempFile new_part_file("new_part.XXXXXX");
    PartitionConfig old_part(kPartitionNameRoot);
    PartitionConfig new_part(kPartitionNameRoot);
    SetupPartitionConfig(&old_part, old_part_file.path(), 0);
    SetupPartitionConfig(&new_part, new_part_file.path(), 10000);
    WriteZerosToFile(old_part_file.path().c_str(), old_part.size);
    WriteZerosToFile(new_part_file.path().c_str(), new_part.size);

    // Select payload generation strategy based on the config.
    unique_ptr<OperationsGenerator> strategy(new FullUpdateGenerator());

    vector<AnnotatedOperation> aops;
    off_t data_file_size = 0;
    ScopedTempFile data_file("temp_data.XXXXXX", true);
    BlobFileWriter blob_file_writer(data_file.fd(), &data_file_size);
    // Generate the operations using the strategy we selected above.
    EXPECT_TRUE(strategy->GenerateOperations(
        config, old_part, new_part, &blob_file_writer, &aops));

    payload.AddPartition(old_part, new_part, aops, {}, 0);

    uint64_t metadata_size;
    EXPECT_TRUE(payload.WritePayload(
        payload_file_.path(), data_file.path(), "", &metadata_size));
  }

  ScopedTempFile payload_file_{"payload_file.XXXXXX"};
};

// Validate the hash of file exists within the output.
TEST_F(PayloadPropertiesTest, GetPropertiesAsJsonTestHash) {
  constexpr char kJsonProperties[] =
      "{"
      R"("is_delta":true,)"
      R"("metadata_signature":"",)"
      R"("metadata_size":165,)"
      R"("sha256_hex":"cV7kfZBH3K0B6QJHxxykDh6b6x0WgVOmc63whPLOy7U=",)"
      R"("size":211,)"
      R"("version":2)"
      "}";
  string json;
  EXPECT_TRUE(
      PayloadProperties(payload_file_.path()).GetPropertiesAsJson(&json));
  EXPECT_EQ(kJsonProperties, json) << "JSON contents:\n" << json;
}

// Validate the hash of file and metadata are within the output.
TEST_F(PayloadPropertiesTest, GetPropertiesAsKeyValueTestHash) {
  constexpr char kKeyValueProperties[] =
      "FILE_HASH=cV7kfZBH3K0B6QJHxxykDh6b6x0WgVOmc63whPLOy7U=\n"
      "FILE_SIZE=211\n"
      "METADATA_HASH=aEKYyzJt2E8Gz8fzB+gmekN5mriotZCSq6R+kDfdeV4=\n"
      "METADATA_SIZE=165\n";
  string key_value;
  EXPECT_TRUE(PayloadProperties{payload_file_.path()}.GetPropertiesAsKeyValue(
      &key_value));
  EXPECT_EQ(kKeyValueProperties, key_value) << "Key Value contents:\n"
                                            << key_value;
}

}  // namespace chromeos_update_engine
