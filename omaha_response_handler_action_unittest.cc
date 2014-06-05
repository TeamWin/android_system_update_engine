// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <gtest/gtest.h>

#include "update_engine/constants.h"
#include "update_engine/fake_system_state.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using testing::NiceMock;
using testing::Return;

namespace chromeos_update_engine {

class OmahaResponseHandlerActionTest : public ::testing::Test {
 public:
  // Return true iff the OmahaResponseHandlerAction succeeded.
  // If out is non-NULL, it's set w/ the response from the action.
  bool DoTestCommon(FakeSystemState* fake_system_state,
                    const OmahaResponse& in,
                    const string& boot_dev,
                    const string& deadline_file,
                    InstallPlan* out);
  bool DoTest(const OmahaResponse& in,
              const string& boot_dev,
              const string& deadline_file,
              InstallPlan* out);
};

class OmahaResponseHandlerActionProcessorDelegate
    : public ActionProcessorDelegate {
 public:
  OmahaResponseHandlerActionProcessorDelegate()
      : code_(ErrorCode::kError),
        code_set_(false) {}
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ErrorCode code) {
    if (action->Type() == OmahaResponseHandlerAction::StaticType()) {
      code_ = code;
      code_set_ = true;
    }
  }
  ErrorCode code_;
  bool code_set_;
};

namespace {
const string kLongName =
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "-the_update_a.b.c.d_DELTA_.tgz";
const string kBadVersion = "don't update me";
}  // namespace {}

bool OmahaResponseHandlerActionTest::DoTestCommon(
    FakeSystemState* fake_system_state,
    const OmahaResponse& in,
    const string& boot_dev,
    const string& test_deadline_file,
    InstallPlan* out) {
  ActionProcessor processor;
  OmahaResponseHandlerActionProcessorDelegate delegate;
  processor.set_delegate(&delegate);

  ObjectFeederAction<OmahaResponse> feeder_action;
  feeder_action.set_obj(in);
  if (in.update_exists and in.version != kBadVersion) {
    EXPECT_CALL(*(fake_system_state->mock_prefs()),
                SetString(kPrefsUpdateCheckResponseHash, in.hash))
        .WillOnce(Return(true));
  }

  string current_url = in.payload_urls.size() ? in.payload_urls[0] : "";
  EXPECT_CALL(*(fake_system_state->mock_payload_state()), GetCurrentUrl())
      .WillRepeatedly(Return(current_url));

  OmahaResponseHandlerAction response_handler_action(
      fake_system_state,
      (test_deadline_file.empty() ?
       OmahaResponseHandlerAction::kDeadlineFile : test_deadline_file));
  response_handler_action.set_boot_device(boot_dev);
  BondActions(&feeder_action, &response_handler_action);
  ObjectCollectorAction<InstallPlan> collector_action;
  BondActions(&response_handler_action, &collector_action);
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&response_handler_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_TRUE(!processor.IsRunning())
      << "Update test to handle non-asynch actions";
  if (out)
    *out = collector_action.object();
  EXPECT_TRUE(delegate.code_set_);
  return delegate.code_ == ErrorCode::kSuccess;
}

bool OmahaResponseHandlerActionTest::DoTest(const OmahaResponse& in,
                                            const string& boot_dev,
                                            const string& deadline_file,
                                            InstallPlan* out) {
  FakeSystemState fake_system_state;
  return DoTestCommon(&fake_system_state, in, boot_dev, deadline_file, out);
}

TEST_F(OmahaResponseHandlerActionTest, SimpleTest) {
  string test_deadline_file;
  CHECK(utils::MakeTempFile(
          "omaha_response_handler_action_unittest-XXXXXX",
          &test_deadline_file, NULL));
  ScopedPathUnlinker deadline_unlinker(test_deadline_file);
  {
    OmahaResponse in;
    in.update_exists = true;
    in.version = "a.b.c.d";
    in.payload_urls.push_back("http://foo/the_update_a.b.c.d.tgz");
    in.more_info_url = "http://more/info";
    in.hash = "HASH+";
    in.size = 12;
    in.prompt = false;
    in.deadline = "20101020";
    InstallPlan install_plan;
    EXPECT_TRUE(DoTest(in, "/dev/sda3", test_deadline_file, &install_plan));
    EXPECT_EQ(in.payload_urls[0], install_plan.download_url);
    EXPECT_EQ(in.hash, install_plan.payload_hash);
    EXPECT_EQ("/dev/sda5", install_plan.install_path);
    string deadline;
    EXPECT_TRUE(utils::ReadFile(test_deadline_file, &deadline));
    EXPECT_EQ("20101020", deadline);
    struct stat deadline_stat;
    EXPECT_EQ(0, stat(test_deadline_file.c_str(), &deadline_stat));
    EXPECT_EQ(S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
              deadline_stat.st_mode);
    EXPECT_EQ(in.version, install_plan.version);
  }
  {
    OmahaResponse in;
    in.update_exists = true;
    in.version = "a.b.c.d";
    in.payload_urls.push_back("http://foo/the_update_a.b.c.d.tgz");
    in.more_info_url = "http://more/info";
    in.hash = "HASHj+";
    in.size = 12;
    in.prompt = true;
    InstallPlan install_plan;
    EXPECT_TRUE(DoTest(in, "/dev/sda5", test_deadline_file, &install_plan));
    EXPECT_EQ(in.payload_urls[0], install_plan.download_url);
    EXPECT_EQ(in.hash, install_plan.payload_hash);
    EXPECT_EQ("/dev/sda3", install_plan.install_path);
    string deadline;
    EXPECT_TRUE(utils::ReadFile(test_deadline_file, &deadline) &&
                deadline.empty());
    EXPECT_EQ(in.version, install_plan.version);
  }
  {
    OmahaResponse in;
    in.update_exists = true;
    in.version = "a.b.c.d";
    in.payload_urls.push_back(kLongName);
    in.more_info_url = "http://more/info";
    in.hash = "HASHj+";
    in.size = 12;
    in.prompt = true;
    in.deadline = "some-deadline";
    InstallPlan install_plan;
    EXPECT_TRUE(DoTest(in, "/dev/sda3", test_deadline_file, &install_plan));
    EXPECT_EQ(in.payload_urls[0], install_plan.download_url);
    EXPECT_EQ(in.hash, install_plan.payload_hash);
    EXPECT_EQ("/dev/sda5", install_plan.install_path);
    string deadline;
    EXPECT_TRUE(utils::ReadFile(test_deadline_file, &deadline));
    EXPECT_EQ("some-deadline", deadline);
    EXPECT_EQ(in.version, install_plan.version);
  }
}

TEST_F(OmahaResponseHandlerActionTest, NoUpdatesTest) {
  OmahaResponse in;
  in.update_exists = false;
  InstallPlan install_plan;
  EXPECT_FALSE(DoTest(in, "/dev/sda1", "", &install_plan));
  EXPECT_EQ("", install_plan.download_url);
  EXPECT_EQ("", install_plan.payload_hash);
  EXPECT_EQ("", install_plan.install_path);
  EXPECT_EQ("", install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest, HashChecksForHttpTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.payload_urls.push_back("http://test.should/need/hash.checks.signed");
  in.more_info_url = "http://more/info";
  in.hash = "HASHj+";
  in.size = 12;
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "/dev/sda5", "", &install_plan));
  EXPECT_EQ(in.payload_urls[0], install_plan.download_url);
  EXPECT_EQ(in.hash, install_plan.payload_hash);
  EXPECT_TRUE(install_plan.hash_checks_mandatory);
  EXPECT_EQ(in.version, install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest, HashChecksForHttpsTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.payload_urls.push_back("https://test.should.not/need/hash.checks.signed");
  in.more_info_url = "http://more/info";
  in.hash = "HASHj+";
  in.size = 12;
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "/dev/sda5", "", &install_plan));
  EXPECT_EQ(in.payload_urls[0], install_plan.download_url);
  EXPECT_EQ(in.hash, install_plan.payload_hash);
  EXPECT_FALSE(install_plan.hash_checks_mandatory);
  EXPECT_EQ(in.version, install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest, HashChecksForBothHttpAndHttpsTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.payload_urls.push_back("http://test.should.still/need/hash.checks");
  in.payload_urls.push_back("https://test.should.still/need/hash.checks");
  in.more_info_url = "http://more/info";
  in.hash = "HASHj+";
  in.size = 12;
  InstallPlan install_plan;
  EXPECT_TRUE(DoTest(in, "/dev/sda5", "", &install_plan));
  EXPECT_EQ(in.payload_urls[0], install_plan.download_url);
  EXPECT_EQ(in.hash, install_plan.payload_hash);
  EXPECT_TRUE(install_plan.hash_checks_mandatory);
  EXPECT_EQ(in.version, install_plan.version);
}

TEST_F(OmahaResponseHandlerActionTest, ChangeToMoreStableChannelTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.payload_urls.push_back("https://MoreStableChannelTest");
  in.more_info_url = "http://more/info";
  in.hash = "HASHjk";
  in.size = 15;

  // Create a uniquely named test directory.
  string test_dir;
  ASSERT_TRUE(utils::MakeTempDirectory(
          "omaha_response_handler_action-test-XXXXXX", &test_dir));

  ASSERT_EQ(0, System(string("mkdir -p ") + test_dir + "/etc"));
  ASSERT_EQ(0, System(string("mkdir -p ") + test_dir +
                      kStatefulPartition + "/etc"));
  ASSERT_TRUE(WriteFileString(
      test_dir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_TRACK=canary-channel\n"));
  ASSERT_TRUE(WriteFileString(
      test_dir + kStatefulPartition + "/etc/lsb-release",
      "CHROMEOS_IS_POWERWASH_ALLOWED=true\n"
      "CHROMEOS_RELEASE_TRACK=stable-channel\n"));

  FakeSystemState fake_system_state;
  OmahaRequestParams params(&fake_system_state);
  params.set_root(test_dir);
  params.SetLockDown(false);
  params.Init("1.2.3.4", "", 0);
  EXPECT_EQ("canary-channel", params.current_channel());
  EXPECT_EQ("stable-channel", params.target_channel());
  EXPECT_TRUE(params.to_more_stable_channel());
  EXPECT_TRUE(params.is_powerwash_allowed());

  fake_system_state.set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTestCommon(&fake_system_state, in, "/dev/sda5", "",
                           &install_plan));
  EXPECT_TRUE(install_plan.powerwash_required);

  ASSERT_TRUE(utils::RecursiveUnlinkDir(test_dir));
}

TEST_F(OmahaResponseHandlerActionTest, ChangeToLessStableChannelTest) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.payload_urls.push_back("https://LessStableChannelTest");
  in.more_info_url = "http://more/info";
  in.hash = "HASHjk";
  in.size = 15;

  // Create a uniquely named test directory.
  string test_dir;
  ASSERT_TRUE(utils::MakeTempDirectory(
          "omaha_response_handler_action-test-XXXXXX", &test_dir));

  ASSERT_EQ(0, System(string("mkdir -p ") + test_dir + "/etc"));
  ASSERT_EQ(0, System(string("mkdir -p ") + test_dir +
                      kStatefulPartition + "/etc"));
  ASSERT_TRUE(WriteFileString(
      test_dir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_TRACK=stable-channel\n"));
  ASSERT_TRUE(WriteFileString(
      test_dir + kStatefulPartition + "/etc/lsb-release",
      "CHROMEOS_RELEASE_TRACK=canary-channel\n"));

  FakeSystemState fake_system_state;
  OmahaRequestParams params(&fake_system_state);
  params.set_root(test_dir);
  params.SetLockDown(false);
  params.Init("5.6.7.8", "", 0);
  EXPECT_EQ("stable-channel", params.current_channel());
  params.SetTargetChannel("canary-channel", false);
  EXPECT_EQ("canary-channel", params.target_channel());
  EXPECT_FALSE(params.to_more_stable_channel());
  EXPECT_FALSE(params.is_powerwash_allowed());

  fake_system_state.set_request_params(&params);
  InstallPlan install_plan;
  EXPECT_TRUE(DoTestCommon(&fake_system_state, in, "/dev/sda5", "",
                           &install_plan));
  EXPECT_FALSE(install_plan.powerwash_required);

  ASSERT_TRUE(utils::RecursiveUnlinkDir(test_dir));
}

TEST_F(OmahaResponseHandlerActionTest, P2PUrlIsUsedAndHashChecksMandatory) {
  OmahaResponse in;
  in.update_exists = true;
  in.version = "a.b.c.d";
  in.payload_urls.push_back("https://would.not/cause/hash/checks");
  in.more_info_url = "http://more/info";
  in.hash = "HASHj+";
  in.size = 12;

  FakeSystemState fake_system_state;
  OmahaRequestParams params(&fake_system_state);
  fake_system_state.set_request_params(&params);

  EXPECT_CALL(*fake_system_state.mock_payload_state(),
              SetUsingP2PForDownloading(true));

  string p2p_url = "http://9.8.7.6/p2p";
  params.set_p2p_url(p2p_url);
  params.set_use_p2p_for_downloading(true);

  InstallPlan install_plan;
  EXPECT_TRUE(DoTestCommon(&fake_system_state, in, "/dev/sda5", "",
                           &install_plan));
  EXPECT_EQ(in.hash, install_plan.payload_hash);
  EXPECT_EQ(install_plan.download_url, p2p_url);
  EXPECT_TRUE(install_plan.hash_checks_mandatory);
}

}  // namespace chromeos_update_engine
