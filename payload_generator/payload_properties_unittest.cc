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

using chromeos_update_engine::test_utils::ScopedTempFile;
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
    config.source.image_info.set_version("123.0.0");
    config.target.image_info.set_version("456.7.8");
    PayloadFile payload;
    EXPECT_TRUE(payload.Init(config));

    const string kTempFileTemplate = "temp_data.XXXXXX";
    int data_file_fd;
    string temp_file_path;
    EXPECT_TRUE(
        utils::MakeTempFile(kTempFileTemplate, &temp_file_path, &data_file_fd));
    ScopedPathUnlinker temp_file_unlinker(temp_file_path);
    EXPECT_LE(0, data_file_fd);

    const auto SetupPartitionConfig =
        [](PartitionConfig* config, const string& path, size_t size) {
          config->path = path;
          config->size = size;
        };
    const auto WriteZerosToFile = [](const char path[], size_t size) {
      string zeros(size, '\0');
      EXPECT_TRUE(utils::WriteFile(path, zeros.c_str(), zeros.size()));
    };
    ScopedTempFile old_part_file;
    ScopedTempFile new_part_file;
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
    BlobFileWriter blob_file_writer(data_file_fd, &data_file_size);
    // Generate the operations using the strategy we selected above.
    EXPECT_TRUE(strategy->GenerateOperations(
        config, old_part, new_part, &blob_file_writer, &aops));

    payload.AddPartition(old_part, new_part, aops);

    uint64_t metadata_size;
    EXPECT_TRUE(payload.WritePayload(
        payload_file.path(), temp_file_path, "", &metadata_size));
  }

  ScopedTempFile payload_file;
};

// Validate the hash of file exists within the output.
TEST_F(PayloadPropertiesTest, GetPropertiesAsJsonTestHash) {
  constexpr char kJsonProperties[] =
      "{"
      R"("is_delta":true,)"
      R"("metadata_signature":"",)"
      R"("metadata_size":187,)"
      R"("sha256_hex":"Rtrj9v3xXhrAi1741HAojtGxAQEOZ7mDyhzskIF4PJc=",)"
      R"("size":233,)"
      R"("source_version":"123.0.0",)"
      R"("target_version":"456.7.8",)"
      R"("version":2)"
      "}";
  string json;
  EXPECT_TRUE(
      PayloadProperties(payload_file.path()).GetPropertiesAsJson(&json));
  EXPECT_EQ(kJsonProperties, json) << "JSON contents:\n" << json;
}

// Validate the hash of file and metadata are within the output.
TEST_F(PayloadPropertiesTest, GetPropertiesAsKeyValueTestHash) {
  constexpr char kKeyValueProperties[] =
      "FILE_HASH=Rtrj9v3xXhrAi1741HAojtGxAQEOZ7mDyhzskIF4PJc=\n"
      "FILE_SIZE=233\n"
      "METADATA_HASH=kiXTexy/s2aPttf4+r8KRZWYZ6FYvwhU6rJGcnnI+U0=\n"
      "METADATA_SIZE=187\n";
  string key_value;
  EXPECT_TRUE(PayloadProperties{payload_file.path()}.GetPropertiesAsKeyValue(
      &key_value));
  EXPECT_EQ(kKeyValueProperties, key_value) << "Key Value contents:\n"
                                            << key_value;
}

}  // namespace chromeos_update_engine
