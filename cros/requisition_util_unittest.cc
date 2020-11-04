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

#include "update_engine/cros/requisition_util.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "update_engine/common/test_utils.h"

using chromeos_update_engine::test_utils::WriteFileString;
using std::string;

namespace {

const char kRemoraJSON[] =
    "{\n"
    "   \"the_list\": [ \"val1\", \"val2\" ],\n"
    "   \"enrollment\": {\n"
    "      \"autostart\": true,\n"
    "      \"can_exit\": false,\n"
    "      \"device_requisition\": \"remora\"\n"
    "   },\n"
    "   \"some_String\": \"1337\",\n"
    "   \"some_int\": 42\n"
    "}\n";

const char kNoEnrollmentJSON[] =
    "{\n"
    "   \"the_list\": [ \"val1\", \"val2\" ],\n"
    "   \"enrollment\": {\n"
    "      \"autostart\": true,\n"
    "      \"can_exit\": false,\n"
    "      \"device_requisition\": \"\"\n"
    "   },\n"
    "   \"some_String\": \"1337\",\n"
    "   \"some_int\": 42\n"
    "}\n";
}  // namespace

namespace chromeos_update_engine {

class RequisitionUtilTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(root_dir_.CreateUniqueTempDir()); }

  void WriteJsonToFile(const string& json) {
    path_ =
        base::FilePath(root_dir_.GetPath().value() + "/chronos/Local State");
    ASSERT_TRUE(base::CreateDirectory(path_.DirName()));
    ASSERT_TRUE(WriteFileString(path_.value(), json));
  }

  base::ScopedTempDir root_dir_;
  base::FilePath path_;
};

TEST_F(RequisitionUtilTest, BadJsonReturnsEmpty) {
  WriteJsonToFile("this isn't JSON");
  EXPECT_EQ("", ReadDeviceRequisition(path_));
}

TEST_F(RequisitionUtilTest, NoFileReturnsEmpty) {
  EXPECT_EQ("", ReadDeviceRequisition(path_));
}

TEST_F(RequisitionUtilTest, EnrollmentRequisition) {
  WriteJsonToFile(kRemoraJSON);
  EXPECT_EQ("remora", ReadDeviceRequisition(path_));
}

TEST_F(RequisitionUtilTest, BlankEnrollment) {
  WriteJsonToFile(kNoEnrollmentJSON);
  EXPECT_EQ("", ReadDeviceRequisition(path_));
}

}  // namespace chromeos_update_engine
