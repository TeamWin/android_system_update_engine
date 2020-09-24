//
// Copyright (C) 2020 The Android Open Source Project
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "update_engine/common/error_code.h"
#include "update_engine/hardware_android.h"

using ::testing::NiceMock;
using ::testing::Return;

namespace chromeos_update_engine {

TEST(HardwareAndroidTest, IsKernelUpdateValid) {
  EXPECT_EQ(ErrorCode::kSuccess,
            HardwareAndroid::IsKernelUpdateValid(
                "5.4.42-not-gki", "", true /*prevent_downgrade*/))
      << "Legacy update should be fine";

  EXPECT_EQ(
      ErrorCode::kSuccess,
      HardwareAndroid::IsKernelUpdateValid(
          "5.4.42-not-gki", "5.4.42-android12-0", true /*prevent_downgrade*/))
      << "Update to GKI should be fine";

  EXPECT_EQ(ErrorCode::kDownloadManifestParseError,
            HardwareAndroid::IsKernelUpdateValid(
                "5.4.42-not-gki", "5.4.42-not-gki", true /*prevent_downgrade*/))
      << "Should report parse error for invalid version field";

  EXPECT_EQ(ErrorCode::kSuccess,
            HardwareAndroid::IsKernelUpdateValid("5.4.42-android12-0-something",
                                                 "5.4.42-android12-0-something",
                                                 true /*prevent_downgrade*/))
      << "Self update should be fine";

  EXPECT_EQ(ErrorCode::kSuccess,
            HardwareAndroid::IsKernelUpdateValid("5.4.42-android12-0-something",
                                                 "5.4.43-android12-0-something",
                                                 true /*prevent_downgrade*/))
      << "Sub-level update should be fine";

  EXPECT_EQ(
      ErrorCode::kSuccess,
      HardwareAndroid::IsKernelUpdateValid("5.4.42-android12-0-something",
                                           "5.10.10-android12-0-something",
                                           true /*prevent_downgrade*/))
      << "KMI version update should be fine";

  EXPECT_EQ(ErrorCode::kPayloadTimestampError,
            HardwareAndroid::IsKernelUpdateValid("5.4.42-android12-0-something",
                                                 "5.4.5-android12-0-something",
                                                 true /*prevent_downgrade*/))
      << "Should detect sub-level downgrade";

  EXPECT_EQ(ErrorCode::kPayloadTimestampError,
            HardwareAndroid::IsKernelUpdateValid("5.4.42-android12-0-something",
                                                 "5.1.5-android12-0-something",
                                                 true /*prevent_downgrade*/))
      << "Should detect KMI version downgrade";

  EXPECT_EQ(ErrorCode::kSuccess,
            HardwareAndroid::IsKernelUpdateValid("5.4.42-android12-0-something",
                                                 "5.4.5-android12-0-something",
                                                 false /*prevent_downgrade*/))
      << "Should suppress sub-level downgrade";
}

}  // namespace chromeos_update_engine
