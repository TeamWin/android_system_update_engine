//
// Copyright (C) 2021 The Android Open Source Project
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

#include <utility>
#include <filesystem>

#include "update_engine/aosp/apex_handler_android.h"

#include <android-base/file.h>
#include <android-base/strings.h>
#include <gtest/gtest.h>

using android::base::EndsWith;

namespace chromeos_update_engine {

namespace fs = std::filesystem;

class ApexHandlerAndroidTest : public ::testing::Test {
 protected:
  ApexHandlerAndroidTest() = default;

  android::sp<android::apex::IApexService> GetApexService() const {
    return apex_handler_.GetApexService();
  }

  uint64_t CalculateSize(
      const std::vector<ApexInfo>& apex_infos,
      android::sp<android::apex::IApexService> apex_service) const {
    return apex_handler_.CalculateSize(apex_infos, apex_service);
  }

  bool AllocateSpace(const uint64_t size_required,
                     const std::string& dir_path) const {
    return apex_handler_.AllocateSpace(size_required, dir_path);
  }

  ApexInfo CreateApexInfo(const std::string& package_name,
                          int version,
                          bool is_compressed,
                          int decompressed_size) {
    ApexInfo result;
    result.set_package_name(package_name);
    result.set_version(version);
    result.set_is_compressed(is_compressed);
    result.set_decompressed_size(decompressed_size);
    return std::move(result);
  }

  ApexHandlerAndroid apex_handler_;
};

// TODO(b/172911822): Once apexd has more optimized response for CalculateSize,
//  improve this test
TEST_F(ApexHandlerAndroidTest, CalculateSize) {
  std::vector<ApexInfo> apex_infos;
  ApexInfo compressed_apex_1 = CreateApexInfo("sample1", 1, true, 10);
  ApexInfo compressed_apex_2 = CreateApexInfo("sample2", 2, true, 20);
  apex_infos.push_back(compressed_apex_1);
  apex_infos.push_back(compressed_apex_2);
  auto apex_service = GetApexService();
  EXPECT_TRUE(apex_service != nullptr) << "Apexservice not found";
  int required_size = CalculateSize(apex_infos, apex_service);
  EXPECT_EQ(required_size, 30);
}

TEST_F(ApexHandlerAndroidTest, AllocateSpace) {
  // Allocating 0 space should be a no op
  TemporaryDir td;
  EXPECT_TRUE(AllocateSpace(0, td.path));
  EXPECT_TRUE(fs::is_empty(td.path));

  // Allocating non-zero space should create a file with tmp suffix
  EXPECT_TRUE(AllocateSpace(2 << 20, td.path));
  EXPECT_FALSE(fs::is_empty(td.path));
  int num_of_file = 0;
  for (const auto& entry : fs::directory_iterator(td.path)) {
    num_of_file++;
    EXPECT_TRUE(EndsWith(entry.path().string(), ".tmp"));
    EXPECT_EQ(fs::file_size(entry.path()), 2u << 20);
  }
  EXPECT_EQ(num_of_file, 1);

  // AllocateSpace should be safe to call twice
  EXPECT_TRUE(AllocateSpace(100, td.path));
  EXPECT_FALSE(fs::is_empty(td.path));
  num_of_file = 0;
  for (const auto& entry : fs::directory_iterator(td.path)) {
    num_of_file++;
    EXPECT_TRUE(EndsWith(entry.path().string(), ".tmp"));
    EXPECT_EQ(fs::file_size(entry.path()), 100u);
  }
  EXPECT_EQ(num_of_file, 1);
}

}  // namespace chromeos_update_engine
