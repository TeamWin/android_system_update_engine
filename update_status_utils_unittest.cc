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

#include "update_engine/update_status_utils.h"

#include <string>

#include <gtest/gtest.h>

using std::string;

namespace chromeos_update_engine {

TEST(UpdateStatusUtilsTest, UpdateEngineStatusToStringTest) {
  // Keep field assignments in same order as they were declared,
  // to prevent compiler warning, -Wreorder-init-fields.
  update_engine::UpdateEngineStatus update_engine_status = {
      .last_checked_time = 156000000,
      .status = update_engine::UpdateStatus::CHECKING_FOR_UPDATE,
      .progress = 0.5,
      .new_size_bytes = 888,
      .new_version = "12345.0.0",
      .is_enterprise_rollback = true,
      .is_install = true,
      .will_powerwash_after_reboot = true,
  };
  string print =
      R"(CURRENT_OP=UPDATE_STATUS_CHECKING_FOR_UPDATE
IS_ENTERPRISE_ROLLBACK=true
IS_INSTALL=true
LAST_CHECKED_TIME=156000000
NEW_SIZE=888
NEW_VERSION=12345.0.0
PROGRESS=0.5
WILL_POWERWASH_AFTER_REBOOT=true
)";
  EXPECT_EQ(print, UpdateEngineStatusToString(update_engine_status));
}

}  // namespace chromeos_update_engine
