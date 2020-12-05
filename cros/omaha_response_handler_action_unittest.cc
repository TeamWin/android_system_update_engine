//
// Copyright (C) 2011 The Android Open Source Project
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

#include "update_engine/cros/omaha_response_handler_action.h"

#include <memory>
#include <string>
#include <utility>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <brillo/message_loops/fake_message_loop.h>
#include <gtest/gtest.h>

#include "update_engine/common/constants.h"
#include "update_engine/common/platform_constants.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/cros/fake_system_state.h"
#include "update_engine/cros/mock_payload_state.h"
#include "update_engine/payload_consumer/payload_constants.h"
#include "update_engine/update_manager/mock_policy.h"

using chromeos_update_engine::test_utils::System;
using chromeos_update_engine::test_utils::WriteFileString;
using chromeos_update_manager::EvalStatus;
using chromeos_update_manager::FakeUpdateManager;
using chromeos_update_manager::kRollforwardInfinity;
using chromeos_update_manager::MockPolicy;
using std::string;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

namespace chromeos_update_engine {

class OmahaResponseHandlerActionProcessorDelegate
    : public ActionProcessorDelegate {
 public:
  OmahaResponseHandlerActionProcessorDelegate()
      : code_(ErrorCode::kError), code_set_(false) {}
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ErrorCode code) {
    if (action->Type() == OmahaResponseHandlerAction::StaticType()) {
      auto response_handler_action =
          static_cast<OmahaResponseHandlerAction*>(action);
      code_ = code;
      code_set_ = true;
      response_handler_action_install_plan_.reset(
          new InstallPlan(response_handler_action->install_plan_));
    } else if (action->Type() ==
               ObjectCollectorAction<InstallPlan>::StaticType()) {
      auto collector_action =
          static_cast<ObjectCollectorAction<InstallPlan>*>(action);
      collector_action_install_plan_.reset(
          new InstallPlan(collector_action->object()));
    }
  }
  ErrorCode code_;
  bool code_set_;
  std::unique_ptr<InstallPlan> collector_action_install_plan_;
  std::unique_ptr<InstallPlan> response_handler_action_install_plan_;
};

class OmahaResponseHandlerActionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FakeSystemState::CreateInstance();
    // Enable MockPrefs;
    FakeSystemState::Get()->set_prefs(nullptr);
    FakeBootControl* fake_boot_control =
        FakeSystemState::Get()->fake_boot_control();
    fake_boot_control->SetPartitionDevice(kPartitionNameKernel, 0, "/dev/sdz2");
    fake_boot_control->SetPartitionDevice(kPartitionNameRoot, 0, "/dev/sdz3");
    fake_boot_control->SetPartitionDevice(kPartitionNameKernel, 1, "/dev/sdz4");
    fake_boot_control->SetPartitionDevice(kPartitionNameRoot, 1, "/dev/sdz5");
  }

  // Return true iff the OmahaResponseHandlerAction succeeded.
  // If out is non-null, it's set w/ the response from the action.
  bool DoTest(const OmahaResponse& in,
              const string& deadline_file,
              InstallPlan* out);

  // Delegate passed to the ActionProcessor.
  OmahaResponseHandlerActionProcessorDelegate delegate_;

  // Captures the action's result code, for tests that need to directly verify
  // it in non-success cases.
  ErrorCode action_result_code_;

  // "Hash+"
  const brillo::Blob expected_hash_ = {0x48, 0x61, 0x73, 0x68, 0x2b};
};

namespace {
const char* const kLongName =
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "-the_update_a.b.c.d_DELTA_.tgz";
const char* const kBadVersion = "don't update me";
const char* const kPayloadHashHex = "486173682b";
const char* const kPayloadFp1 = "1.755aff78ec73dfc7f590893ac";
const char* const kPayloadFp2 = "1.98ba213e0ccec0d0e8cdc74a5";
const char* const kPayloadAppId = "test_app_id";
}  // namespace

bool OmahaResponseHandlerActionTest::DoTest(const OmahaResponse& in,
                                            const string& test_deadline_file,
                                            InstallPlan* out) {
  brillo::FakeMessageLoop loop(nullptr);
  loop.SetAsCurrent();
  ActionProcessor processor;
  processor.set_delegate(&delegate_);

  auto feeder_action = std::make_unique<ObjectFeederAction<OmahaResponse>>();
  feeder_action->set_obj(in);
  if (in.update_exists && in.version != kBadVersion) {
    string expected_hash;
    for (const auto& package : in.packages)
      expected_hash += package.hash + ":";
    EXPECT_CALL(*(FakeSystemState::Get()->mock_prefs()),
                SetString(kPrefsUpdateCheckResponseHash, expected_hash))
        .WillOnce(Return(true));

    int slot =
        FakeSystemState::Get()->request_params()->is_install()
            ? FakeSystemState::Get()->fake_boot_control()->GetCurrentSlot()
            : 1 - FakeSystemState::Get()->fake_boot_control()->GetCurrentSlot();
    string key = kPrefsChannelOnSlotPrefix + std::to_string(slot);
    EXPECT_CALL(*(FakeSystemState::Get()->mock_prefs()),
                SetString(key, testing::_))
        .WillOnce(Return(true));
  }

  string current_url = in.packages.size() ? in.packages[0].payload_urls[0] : "";
  EXPECT_CALL(*(FakeSystemState::Get()->mock_payload_state()), GetCurrentUrl())
      .WillRepeatedly(Return(current_url));

  auto response_handler_action = std::make_unique<OmahaResponseHandlerAction>();
  if (!test_deadline_file.empty())
    response_handler_action->deadline_file_ = test_deadline_file;

  auto collector_action =
      std::make_unique<ObjectCollectorAction<InstallPlan>>();

  BondActions(feeder_action.get(), response_handler_action.get());
  BondActions(response_handler_action.get(), collector_action.get());
  processor.EnqueueAction(std::move(feeder_action));
  processor.EnqueueAction(std::move(response_handler_action));
  processor.EnqueueAction(std::move(collector_action));
  processor.StartProcessing();
  EXPECT_TRUE(!processor.IsRunning())
      << "Update test to handle non-async actions";

  if (out && delegate_.collector_action_install_plan_)
    *out = *delegate_.collector_action_install_plan_;

  EXPECT_TRUE(delegate_.code_set_);
  action_result_code_ = delegate_.code_;
  return delegate_.code_ == ErrorCode::kSuccess;
}

TEST_F(OmahaResponseHandlerActionTest, SimpleTest) {
  ScopedTempFile test_deadline_file(
      "omaha_response_handler_action_unittest-XXXXXX");
  {
    OmahaResponse in;
    in.update_exists = true;
    in.version = "a.b.c.d";
    in.packages.push_back(
        {.payload_urls = {"http://foo/the_update_a.b.c.d.tgz"},
         .size = 12,
         .hash = kPayloadHashHex,
         .app_id = kPayloadAppId,
         .fp = kPayloadFp1});
    in.more_info_url = "http://more/info";
    in.prompt = false;
    in.deadline = "20101020";
    InstallPlan install_plan;
    EXPECT_TRUE(DoTest(in, test_deadline_file.path(), &install_plan));
    EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
    EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
    EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
    EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
    EXPECT_EQ(1U, install_plan.target_slot);
    string deadline;
    EXPECT_TRUE(utils::ReadFile(test_deadline_file.path(), &deadline));
    EXPECT_EQ("20101020", deadline);
    struct stat deadline_stat;
    EXPECT_EQ(0, stat(test_deadline_file.path().c_str(), &deadline_stat));
    EXPECT_EQ(
        static_cast<mode_t>(S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
        deadline_stat.st_mode);
    EXPECT_EQ(in.version, install_plan.version);
  }
  {
    OmahaResponse in;
    in.update_exists = true;
    in.version = "a.b.c.d";
    in.packages.push_back(
        {.payload_urls = {"http://foo/the_update_a.b.c.d.tgz"},
         .size = 12,
         .hash = kPayloadHashHex,
         .app_id = kPayloadAppId,
         .fp = kPayloadFp1});
    in.more_info_url = "http://more/info";
    in.prompt = true;
    InstallPlan install_plan;
    // Set the other slot as current.
    FakeSystemState::Get()->fake_boot_control()->SetCurrentSlot(1);
    EXPECT_TRUE(DoTest(in, test_deadline_file.path(), &install_plan));
    EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
    EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
    EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
    EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
    EXPECT_EQ(0U, install_plan.target_slot);
    string deadline;
    EXPECT_TRUE(utils::ReadFile(test_deadline_file.path(), &deadline) &&
                deadline.empty());
    EXPECT_EQ(in.version, install_plan.version);
  }
  {
    OmahaResponse in;
    in.update_exists = true;
    in.version = "a.b.c.d";
    in.packages.push_back({.payload_urls = {kLongName},
                           .size = 12,
                           .hash = kPayloadHashHex,
                           .app_id = kPayloadAppId,
                           .fp = kPayloadFp1});
    in.more_info_url = "http://more/info";
    in.prompt = true;
    in.deadline = "some-deadline";
    InstallPlan install_plan;
    FakeSystemState::Get()->fake_boot_control()->SetCurrentSlot(0);
    // Because rollback happened, the deadline shouldn't be written into the
    // file.
    EXPECT_CALL(*(FakeSystemState::Get()->mock_payload_state()),
                GetRollbackHappened())
        .WillOnce(Return(true));
    EXPECT_TRUE(DoTest(in, test_deadline_file.path(), &install_plan));
    EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
    EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
    EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
    EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
    EXPECT_EQ(1U, install_plan.target_slot);
    string deadline;
    EXPECT_TRUE(utils::ReadFile(test_deadline_file.path(), &deadline));
    EXPECT_TRUE(deadline.empty());
    EXPECT_EQ(in.version, install_plan.version);
  }
  {
    OmahaResponse in;
    in.update_exists = true;
    in.version = "a.b.c.d";
    in.packages.push_back({.payload_urls = {kLongName},
                           .size = 12,
                           .hash = kPayloadHashHex,
                           .app_id = kPayloadAppId,
                           .fp = kPayloadFp1});
    in.more_info_url = "http://more/info";
    in.prompt = true;
    in.deadline = "some-deadline";
    InstallPlan install_plan;
    FakeSystemState::Get()->fake_boot_control()->SetCurrentSlot(0);
    EXPECT_CALL(*(FakeSystemState::Get()->mock_payload_state()),
                GetRollbackHappened())
        .WillOnce(Return(false));
    EXPECT_TRUE(DoTest(in, test_deadline_file.path(), &install_plan));
    EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
    EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
    EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
    EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
    EXPECT_EQ(1U, install_plan.target_slot);
    string deadline;
    EXPECT_TRUE(utils::ReadFile(test_deadline_file.path(), &deadline));
    EXPECT_EQ("some-deadline", deadline);
    EXPECT_EQ(in.version, install_plan.version);
  }
}

TEST_F(OmahaResponseHandlerActionTest, NoUpdatesTest) {
  OmahaResponse in;
  in.update_exists = false;
  InstallPlan install_plan;
  EXPECT_FALSE(DoTest(in, "", &install_plan));
  EXPECT_TRUE(install_plan.partitions.empty());
}

TEST_F(OmahaResponseHandlerActionTest, InstallTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.packages.push_back(
      {.payload_urls = {kLongName}, .size = 1, .hash = kPayloadHashHex});
  in.packages.push_back(
      {.payload_urls = {kLongName}, .size = 2, .hash = kPayloadHashHex});
  in.more_info_url = "http://more/info";

  OmahaRequestParams params;
  params.set_is_install(true);

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_EQ(install_plan.source_slot, UINT_MAX);
}

TEST_F(OmahaResponseHandlerActionTest, MultiPackageTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.packages.push_back({.payload_urls = {"http://package/1"},
                         .size = 1,
                         .hash = kPayloadHashHex,
                         .app_id = kPayloadAppId,
                         .fp = kPayloadFp1});
  in.packages.push_back({.payload_urls = {"http://package/2"},
                         .size = 2,
                         .hash = kPayloadHashHex,
                         .app_id = kPayloadAppId,
                         .fp = kPayloadFp2});
  in.more_info_url = "http://more/info";
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
  EXPECT_EQ(2u, install_plan.payloads.size());
  EXPECT_EQ(in.packages[0].size, install_plan.payloads[0].size);
  EXPECT_EQ(in.packages[1].size, install_plan.payloads[1].size);
  EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
  EXPECT_EQ(expected_hash_, install_plan.payloads[1].hash);
  EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
  EXPECT_EQ(in.packages[1].app_id, install_plan.payloads[1].app_id);
  EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
  EXPECT_EQ(in.packages[1].fp, install_plan.payloads[1].fp);
  EXPECT_EQ(in.version, install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest, HashChecksForHttpTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.packages.push_back(
      {.payload_urls = {"http://test.should/need/hash.checks.signed"},
       .size = 12,
       .hash = kPayloadHashHex,
       .app_id = kPayloadAppId,
       .fp = kPayloadFp1});
  in.more_info_url = "http://more/info";
  // Hash checks are always skipped for non-official update URLs.
  EXPECT_CALL(*(FakeSystemState::Get()->mock_request_params()),
              IsUpdateUrlOfficial())
      .WillRepeatedly(Return(true));
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
  EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
  EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
  EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
  EXPECT_TRUE(install_plan.hash_checks_mandatory);
  EXPECT_EQ(in.version, install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest, HashChecksForUnofficialUpdateUrl) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.packages.push_back(
      {.payload_urls = {"http://url.normally/needs/hash.checks.signed"},
       .size = 12,
       .hash = kPayloadHashHex,
       .app_id = kPayloadAppId,
       .fp = kPayloadFp1});
  in.more_info_url = "http://more/info";
  EXPECT_CALL(*(FakeSystemState::Get()->mock_request_params()),
              IsUpdateUrlOfficial())
      .WillRepeatedly(Return(false));
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
  EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
  EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
  EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
  EXPECT_FALSE(install_plan.hash_checks_mandatory);
  EXPECT_EQ(in.version, install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest,
       HashChecksForOfficialUrlUnofficialBuildTest) {
  // Official URLs for unofficial builds (dev/test images) don't require hash.
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.packages.push_back(
      {.payload_urls = {"http://url.normally/needs/hash.checks.signed"},
       .size = 12,
       .hash = kPayloadHashHex,
       .app_id = kPayloadAppId,
       .fp = kPayloadFp1});
  in.more_info_url = "http://more/info";
  EXPECT_CALL(*(FakeSystemState::Get()->mock_request_params()),
              IsUpdateUrlOfficial())
      .WillRepeatedly(Return(true));
  FakeSystemState::Get()->fake_hardware()->SetIsOfficialBuild(false);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
  EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
  EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
  EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
  EXPECT_FALSE(install_plan.hash_checks_mandatory);
  EXPECT_EQ(in.version, install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest, HashChecksForHttpsTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.packages.push_back(
      {.payload_urls = {"https://test.should/need/hash.checks.signed"},
       .size = 12,
       .hash = kPayloadHashHex,
       .app_id = kPayloadAppId,
       .fp = kPayloadFp1});
  in.more_info_url = "http://more/info";
  EXPECT_CALL(*(FakeSystemState::Get()->mock_request_params()),
              IsUpdateUrlOfficial())
      .WillRepeatedly(Return(true));
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
  EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
  EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
  EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
  EXPECT_TRUE(install_plan.hash_checks_mandatory);
  EXPECT_EQ(in.version, install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest, HashChecksForBothHttpAndHttpsTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.packages.push_back(
      {.payload_urls = {"http://test.should.still/need/hash.checks",
                        "https://test.should.still/need/hash.checks"},
       .size = 12,
       .hash = kPayloadHashHex,
       .app_id = kPayloadAppId,
       .fp = kPayloadFp1});
  in.more_info_url = "http://more/info";
  EXPECT_CALL(*(FakeSystemState::Get()->mock_request_params()),
              IsUpdateUrlOfficial())
      .WillRepeatedly(Return(true));
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
  EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
  EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
  EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
  EXPECT_TRUE(install_plan.hash_checks_mandatory);
  EXPECT_EQ(in.version, install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest,
       ChangeToMoreStableVersionAndChannelTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "1.0.0.0";
  in.packages.push_back({.payload_urls = {"https://MoreStableChannelTest"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.more_info_url = "http://more/info";

  // Create a uniquely named test directory.
  base::ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());

  OmahaRequestParams params;
  FakeSystemState::Get()->fake_hardware()->SetIsOfficialBuild(false);
  params.set_root(tempdir.GetPath().value());
  params.set_current_channel("canary-channel");
  EXPECT_TRUE(params.SetTargetChannel("stable-channel", true, nullptr));
  params.UpdateDownloadChannel();
  params.set_app_version("2.0.0.0");

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_TRUE(install_plan.powerwash_required);
}

TEST_F(OmahaResponseHandlerActionTest,
       ChangeToMoreStableVersionAndChannelPowerwashNotAllowedTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "1.0.0.0";
  in.packages.push_back({.payload_urls = {"https://MoreStableChannelTest"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.more_info_url = "http://more/info";

  // Create a uniquely named test directory.
  base::ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());

  OmahaRequestParams params;
  FakeSystemState::Get()->fake_hardware()->SetIsOfficialBuild(false);
  params.set_root(tempdir.GetPath().value());
  params.set_current_channel("canary-channel");
  EXPECT_TRUE(params.SetTargetChannel("stable-channel", false, nullptr));
  params.UpdateDownloadChannel();
  params.set_app_version("2.0.0.0");

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_FALSE(install_plan.powerwash_required);
}

TEST_F(OmahaResponseHandlerActionTest,
       ChangeToMoreStableChannelButNewerVersionTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "12345.96.0.0";
  in.packages.push_back({.payload_urls = {"https://ChannelDownVersionUp"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.more_info_url = "http://more/info";

  // Create a uniquely named test directory.
  base::ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());

  OmahaRequestParams params;
  FakeSystemState::Get()->fake_hardware()->SetIsOfficialBuild(false);
  params.set_root(tempdir.GetPath().value());
  params.set_current_channel("beta-channel");
  EXPECT_TRUE(params.SetTargetChannel("stable-channel", true, nullptr));
  params.UpdateDownloadChannel();
  params.set_app_version("12345.48.0.0");

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_FALSE(install_plan.powerwash_required);
}

TEST_F(OmahaResponseHandlerActionTest,
       ChangeToMoreStableChannelButSameVersionTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "12345.0.0.0";
  in.packages.push_back({.payload_urls = {"https://ChannelDownVersionUp"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.more_info_url = "http://more/info";

  // Create a uniquely named test directory.
  base::ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());

  OmahaRequestParams params;
  FakeSystemState::Get()->fake_hardware()->SetIsOfficialBuild(false);
  params.set_root(tempdir.GetPath().value());
  params.set_current_channel("beta-channel");
  EXPECT_TRUE(params.SetTargetChannel("stable-channel", true, nullptr));
  params.UpdateDownloadChannel();
  params.set_app_version("12345.0.0.0");

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_FALSE(install_plan.powerwash_required);
  EXPECT_FALSE(install_plan.rollback_data_save_requested);
}

// On an enrolled device, the rollback data restore should be attempted when
// doing a powerwash and channel downgrade.
TEST_F(OmahaResponseHandlerActionTest,
       ChangeToMoreStableChannelEnrolledDataRestore) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "12345.96.0.0";
  in.packages.push_back({.payload_urls = {"https://ChannelDownEnrolled"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.more_info_url = "http://more/info";

  // Create a uniquely named test directory.
  base::ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());

  OmahaRequestParams params;
  FakeSystemState::Get()->fake_hardware()->SetIsOfficialBuild(true);
  params.set_root(tempdir.GetPath().value());
  params.set_current_channel("beta-channel");
  EXPECT_TRUE(params.SetTargetChannel("stable-channel", true, nullptr));
  params.UpdateDownloadChannel();
  params.set_app_version("12347.48.0.0");

  testing::NiceMock<policy::MockDevicePolicy> mock_device_policy;
  EXPECT_CALL(mock_device_policy, IsEnterpriseEnrolled())
      .WillOnce(Return(true));
  FakeSystemState::Get()->set_device_policy(&mock_device_policy);

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_TRUE(install_plan.rollback_data_save_requested);
}

// Never attempt rollback data restore if the device is not enrolled.
TEST_F(OmahaResponseHandlerActionTest,
       ChangeToMoreStableChannelUnenrolledNoDataRestore) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "12345.96.0.0";
  in.packages.push_back({.payload_urls = {"https://ChannelDownEnrolled"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.more_info_url = "http://more/info";

  // Create a uniquely named test directory.
  base::ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());

  OmahaRequestParams params;
  FakeSystemState::Get()->fake_hardware()->SetIsOfficialBuild(true);
  params.set_root(tempdir.GetPath().value());
  params.set_current_channel("beta-channel");
  EXPECT_TRUE(params.SetTargetChannel("stable-channel", true, nullptr));
  params.UpdateDownloadChannel();
  params.set_app_version("12347.48.0.0");

  testing::NiceMock<policy::MockDevicePolicy> mock_device_policy;
  EXPECT_CALL(mock_device_policy, IsEnterpriseEnrolled())
      .WillOnce(Return(false));
  FakeSystemState::Get()->set_device_policy(&mock_device_policy);

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_FALSE(install_plan.rollback_data_save_requested);
}

// Never attempt rollback data restore if powerwash is not allowed.
TEST_F(OmahaResponseHandlerActionTest,
       ChangeToMoreStableChannelNoPowerwashNoDataRestore) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "12345.96.0.0";
  in.packages.push_back(
      {.payload_urls = {"https://URL"}, .size = 1, .hash = kPayloadHashHex});
  in.more_info_url = "http://more/info";

  // Create a uniquely named test directory.
  base::ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());

  OmahaRequestParams params;
  FakeSystemState::Get()->fake_hardware()->SetIsOfficialBuild(true);
  params.set_root(tempdir.GetPath().value());
  params.set_current_channel("beta-channel");
  EXPECT_TRUE(params.SetTargetChannel("stable-channel", false, nullptr));
  params.UpdateDownloadChannel();
  params.set_app_version("12347.48.0.0");

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_FALSE(install_plan.rollback_data_save_requested);
}

TEST_F(OmahaResponseHandlerActionTest,
       ChangeToLessStableVersionAndChannelTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "2.0.0.0";
  in.packages.push_back({.payload_urls = {"https://LessStableChannelTest"},
                         .size = 15,
                         .hash = kPayloadHashHex});
  in.more_info_url = "http://more/info";

  // Create a uniquely named test directory.
  base::ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());

  OmahaRequestParams params;
  FakeSystemState::Get()->fake_hardware()->SetIsOfficialBuild(false);
  params.set_root(tempdir.GetPath().value());
  params.set_current_channel("stable-channel");
  EXPECT_TRUE(params.SetTargetChannel("canary-channel", false, nullptr));
  params.UpdateDownloadChannel();
  params.set_app_version("1.0.0.0");

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_FALSE(install_plan.powerwash_required);
}

TEST_F(OmahaResponseHandlerActionTest, P2PUrlIsUsedAndHashChecksMandatory) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.packages.push_back(
      {.payload_urls = {"https://would.not/cause/hash/checks"},
       .size = 12,
       .hash = kPayloadHashHex,
       .app_id = kPayloadAppId,
       .fp = kPayloadFp1});
  in.more_info_url = "http://more/info";

  OmahaRequestParams params;
  // We're using a real OmahaRequestParams object here so we can't mock
  // IsUpdateUrlOfficial(), but setting the update URL to the AutoUpdate test
  // server will cause IsUpdateUrlOfficial() to return true.
  params.set_update_url(constants::kOmahaDefaultAUTestURL);
  FakeSystemState::Get()->set_request_params(&params);

  EXPECT_CALL(*FakeSystemState::Get()->mock_payload_state(),
              SetUsingP2PForDownloading(true));

  string p2p_url = "http://9.8.7.6/p2p";
  EXPECT_CALL(*FakeSystemState::Get()->mock_payload_state(), GetP2PUrl())
      .WillRepeatedly(Return(p2p_url));
  EXPECT_CALL(*FakeSystemState::Get()->mock_payload_state(),
              GetUsingP2PForDownloading())
      .WillRepeatedly(Return(true));

  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
  EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
  EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
  EXPECT_EQ(p2p_url, install_plan.download_url);
  EXPECT_TRUE(install_plan.hash_checks_mandatory);
}

TEST_F(OmahaResponseHandlerActionTest, RollbackTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.packages.push_back({.payload_urls = {"https://RollbackTest"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.is_rollback = true;

  // The rollback payload is 2 versions behind stable.
  in.rollback_key_version.kernel = 24;
  in.rollback_key_version.kernel = 23;
  in.rollback_key_version.firmware_key = 22;
  in.rollback_key_version.firmware = 21;

  OmahaResponse::RollbackKeyVersion m4;
  m4.firmware_key = 16;
  m4.firmware = 15;
  m4.kernel_key = 14;
  m4.kernel = 13;

  in.past_rollback_key_version = m4;

  FakeSystemState::Get()->fake_hardware()->SetMinKernelKeyVersion(0x00010002);
  FakeSystemState::Get()->fake_hardware()->SetMinFirmwareKeyVersion(0x00030004);

  FakeSystemState::Get()->fake_hardware()->SetMaxKernelKeyRollforward(
      0xaaaaaaaa);
  // TODO(crbug/783998): Add support for firmware when implemented.

  OmahaRequestParams params;
  params.set_rollback_allowed(true);
  params.set_rollback_allowed_milestones(4);

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_TRUE(install_plan.is_rollback);

  // The max rollforward should be set the values of the image
  // rollback_allowed_milestones (4 for this test) in the past.
  const uint32_t expected_max_kernel_rollforward =
      static_cast<uint32_t>(m4.kernel_key) << 16 |
      static_cast<uint32_t>(m4.kernel);
  EXPECT_EQ(
      expected_max_kernel_rollforward,
      FakeSystemState::Get()->fake_hardware()->GetMaxKernelKeyRollforward());
  // TODO(crbug/783998): Add support for firmware when implemented.
}

TEST_F(OmahaResponseHandlerActionTest, RollbackKernelVersionErrorTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.packages.push_back({.payload_urls = {"https://RollbackTest"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.is_rollback = true;
  in.rollback_key_version.kernel_key = 1;
  in.rollback_key_version.kernel = 1;  // This is lower than the minimum.
  in.rollback_key_version.firmware_key = 3;
  in.rollback_key_version.firmware = 4;

  OmahaResponse::RollbackKeyVersion m4;
  m4.firmware_key = 16;
  m4.firmware = 15;
  m4.kernel_key = 14;
  m4.kernel = 13;
  in.past_rollback_key_version = m4;

  FakeSystemState::Get()->fake_hardware()->SetMinKernelKeyVersion(0x00010002);
  FakeSystemState::Get()->fake_hardware()->SetMinFirmwareKeyVersion(0x00030004);
  const uint32_t current_kernel_max_rollforward = 0xaaaaaaaa;
  FakeSystemState::Get()->fake_hardware()->SetMaxKernelKeyRollforward(
      current_kernel_max_rollforward);

  OmahaRequestParams params;
  params.set_rollback_allowed(true);
  params.set_rollback_allowed_milestones(4);

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_FALSE(DoTest(in, "", &install_plan));

  // Max rollforward is not changed in error cases.
  EXPECT_EQ(
      current_kernel_max_rollforward,
      FakeSystemState::Get()->fake_hardware()->GetMaxKernelKeyRollforward());
  // TODO(crbug/783998): Add support for firmware when implemented.
}

TEST_F(OmahaResponseHandlerActionTest, RollbackFirmwareVersionErrorTest) {
  // TODO(crbug/783998): Add handling for max_firmware_rollforward when
  // implemented.
  OmahaResponse in;
  in.update_exists = true;
  in.packages.push_back({.payload_urls = {"https://RollbackTest"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.is_rollback = true;
  in.rollback_key_version.kernel_key = 1;
  in.rollback_key_version.kernel = 2;
  in.rollback_key_version.firmware_key = 3;
  in.rollback_key_version.firmware = 3;  // This is lower than the minimum.

  FakeSystemState::Get()->fake_hardware()->SetMinKernelKeyVersion(0x00010002);
  FakeSystemState::Get()->fake_hardware()->SetMinFirmwareKeyVersion(0x00030004);

  OmahaRequestParams params;
  params.set_rollback_allowed(true);
  params.set_rollback_allowed_milestones(4);

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_FALSE(DoTest(in, "", &install_plan));
}

TEST_F(OmahaResponseHandlerActionTest, RollbackNotRollbackTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.packages.push_back({.payload_urls = {"https://RollbackTest"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.is_rollback = false;

  const uint32_t current_kernel_max_rollforward = 0xaaaaaaaa;
  FakeSystemState::Get()->fake_hardware()->SetMaxKernelKeyRollforward(
      current_kernel_max_rollforward);

  OmahaRequestParams params;
  params.set_rollback_allowed(true);
  params.set_rollback_allowed_milestones(4);

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_FALSE(install_plan.is_rollback);

  // Max rollforward is not changed for non-rollback cases.
  EXPECT_EQ(
      current_kernel_max_rollforward,
      FakeSystemState::Get()->fake_hardware()->GetMaxKernelKeyRollforward());
  // TODO(crbug/783998): Add support for firmware when implemented.
}

TEST_F(OmahaResponseHandlerActionTest, RollbackNotAllowedTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.packages.push_back({.payload_urls = {"https://RollbackTest"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.is_rollback = true;

  OmahaRequestParams params;
  params.set_rollback_allowed(false);
  params.set_rollback_allowed_milestones(4);

  const uint32_t current_kernel_max_rollforward = 0xaaaaaaaa;
  FakeSystemState::Get()->fake_hardware()->SetMaxKernelKeyRollforward(
      current_kernel_max_rollforward);

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_FALSE(DoTest(in, "", &install_plan));

  // This case generates an error so, do not update max rollforward.
  EXPECT_EQ(
      current_kernel_max_rollforward,
      FakeSystemState::Get()->fake_hardware()->GetMaxKernelKeyRollforward());
  // TODO(crbug/783998): Add support for firmware when implemented.
}

TEST_F(OmahaResponseHandlerActionTest, NormalUpdateWithZeroMilestonesAllowed) {
  OmahaResponse in;
  in.update_exists = true;
  in.packages.push_back({.payload_urls = {"https://RollbackTest"},
                         .size = 1,
                         .hash = kPayloadHashHex});
  in.is_rollback = false;

  OmahaRequestParams params;
  params.set_rollback_allowed(true);
  params.set_rollback_allowed_milestones(0);

  const uint32_t current_kernel_max_rollforward = 0xaaaaaaaa;
  FakeSystemState::Get()->fake_hardware()->SetMaxKernelKeyRollforward(
      current_kernel_max_rollforward);

  FakeSystemState::Get()->set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));

  // When allowed_milestones is 0, this is set to infinity.
  EXPECT_EQ(
      kRollforwardInfinity,
      FakeSystemState::Get()->fake_hardware()->GetMaxKernelKeyRollforward());
  // TODO(crbug/783998): Add support for firmware when implemented.
}

TEST_F(OmahaResponseHandlerActionTest, SystemVersionTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.packages.push_back({.payload_urls = {"http://package/1"},
                         .size = 1,
                         .hash = kPayloadHashHex,
                         .app_id = kPayloadAppId,
                         .fp = kPayloadFp1});
  in.packages.push_back({.payload_urls = {"http://package/2"},
                         .size = 2,
                         .hash = kPayloadHashHex,
                         .app_id = kPayloadAppId,
                         .fp = kPayloadFp2});
  in.more_info_url = "http://more/info";
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "", &install_plan));
  EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
  EXPECT_EQ(2u, install_plan.payloads.size());
  EXPECT_EQ(in.packages[0].size, install_plan.payloads[0].size);
  EXPECT_EQ(in.packages[1].size, install_plan.payloads[1].size);
  EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
  EXPECT_EQ(expected_hash_, install_plan.payloads[1].hash);
  EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
  EXPECT_EQ(in.packages[1].app_id, install_plan.payloads[1].app_id);
  EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
  EXPECT_EQ(in.packages[1].fp, install_plan.payloads[1].fp);
  EXPECT_EQ(in.version, install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest, TestDeferredByPolicy) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.packages.push_back({.payload_urls = {"http://foo/the_update_a.b.c.d.tgz"},
                         .size = 12,
                         .hash = kPayloadHashHex,
                         .app_id = kPayloadAppId,
                         .fp = kPayloadFp1});
  // Setup the UpdateManager to disallow the update.
  MockPolicy* mock_policy = new MockPolicy();
  FakeUpdateManager* fake_update_manager =
      FakeSystemState::Get()->fake_update_manager();
  fake_update_manager->set_policy(mock_policy);
  EXPECT_CALL(*mock_policy, UpdateCanBeApplied(_, _, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<3>(ErrorCode::kOmahaUpdateDeferredPerPolicy),
                Return(EvalStatus::kSucceeded)));
  // Perform the Action. It should "fail" with kOmahaUpdateDeferredPerPolicy.
  InstallPlan install_plan;
  EXPECT_FALSE(DoTest(in, "", &install_plan));
  EXPECT_EQ(ErrorCode::kOmahaUpdateDeferredPerPolicy, action_result_code_);
  // Verify that DoTest() didn't set the output install plan.
  EXPECT_EQ("", install_plan.version);
  // Now verify the InstallPlan that was generated.
  install_plan = *delegate_.response_handler_action_install_plan_;
  EXPECT_EQ(in.packages[0].payload_urls[0], install_plan.download_url);
  EXPECT_EQ(expected_hash_, install_plan.payloads[0].hash);
  EXPECT_EQ(in.packages[0].app_id, install_plan.payloads[0].app_id);
  EXPECT_EQ(in.packages[0].fp, install_plan.payloads[0].fp);
  EXPECT_EQ(1U, install_plan.target_slot);
  EXPECT_EQ(in.version, install_plan.version);
}

}  // namespace chromeos_update_engine
