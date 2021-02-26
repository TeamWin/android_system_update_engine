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

TEST(ApexHandlerAndroidTest, CalculateSize) {
  ApexHandlerAndroid apex_handler;
  std::vector<ApexInfo> apex_infos;
  ApexInfo compressed_apex_1 = CreateApexInfo("sample1", 1, true, 1);
  ApexInfo compressed_apex_2 = CreateApexInfo("sample2", 2, true, 2);
  ApexInfo uncompressed_apex = CreateApexInfo("uncompressed", 1, false, 4);
  apex_infos.push_back(compressed_apex_1);
  apex_infos.push_back(compressed_apex_2);
  apex_infos.push_back(uncompressed_apex);
  auto result = apex_handler.CalculateSize(apex_infos);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, 3u);
}

TEST(ApexHandlerAndroidTest, AllocateSpace) {
  ApexHandlerAndroid apex_handler;
  std::vector<ApexInfo> apex_infos;
  ApexInfo compressed_apex_1 = CreateApexInfo("sample1", 1, true, 1);
  ApexInfo compressed_apex_2 = CreateApexInfo("sample2", 2, true, 2);
  ApexInfo uncompressed_apex = CreateApexInfo("uncompressed", 1, false, 4);
  apex_infos.push_back(compressed_apex_1);
  apex_infos.push_back(compressed_apex_2);
  apex_infos.push_back(uncompressed_apex);
  EXPECT_TRUE(apex_handler.AllocateSpace(apex_infos));

  // Should be able to pass empty list
  EXPECT_TRUE(apex_handler.AllocateSpace({}));
}

}  // namespace chromeos_update_engine
