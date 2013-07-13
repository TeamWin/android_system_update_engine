// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/file_util.h>
#include <gtest/gtest.h>
#include <policy/libpolicy.h>
#include <policy/mock_device_policy.h>

#include "update_engine/action_mock.h"
#include "update_engine/action_processor_mock.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/install_plan.h"
#include "update_engine/mock_dbus_interface.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/mock_payload_state.h"
#include "update_engine/mock_system_state.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/prefs.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_attempter.h"
#include "update_engine/update_check_scheduler.h"

using std::string;
using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Ne;
using testing::NiceMock;
using testing::Property;
using testing::Return;
using testing::SetArgumentPointee;

namespace chromeos_update_engine {

// Test a subclass rather than the main class directly so that we can mock out
// methods within the class. There're explicit unit tests for the mocked out
// methods.
class UpdateAttempterUnderTest : public UpdateAttempter {
 public:
  explicit UpdateAttempterUnderTest(MockSystemState* mock_system_state,
                                    MockDbusGlib* dbus)
      : UpdateAttempter(mock_system_state, dbus) {}
};

class UpdateAttempterTest : public ::testing::Test {
 protected:
  UpdateAttempterTest()
      : attempter_(&mock_system_state_, &dbus_),
        mock_connection_manager(&mock_system_state_),
        loop_(NULL) {
    mock_system_state_.set_connection_manager(&mock_connection_manager);
  }
  virtual void SetUp() {
    EXPECT_EQ(NULL, attempter_.dbus_service_);
    EXPECT_TRUE(attempter_.system_state_ != NULL);
    EXPECT_EQ(NULL, attempter_.update_check_scheduler_);
    EXPECT_EQ(0, attempter_.http_response_code_);
    EXPECT_EQ(utils::kCpuSharesNormal, attempter_.shares_);
    EXPECT_EQ(NULL, attempter_.manage_shares_source_);
    EXPECT_FALSE(attempter_.download_active_);
    EXPECT_EQ(UPDATE_STATUS_IDLE, attempter_.status_);
    EXPECT_EQ(0.0, attempter_.download_progress_);
    EXPECT_EQ(0, attempter_.last_checked_time_);
    EXPECT_EQ("0.0.0.0", attempter_.new_version_);
    EXPECT_EQ(0, attempter_.new_payload_size_);
    processor_ = new NiceMock<ActionProcessorMock>();
    attempter_.processor_.reset(processor_);  // Transfers ownership.
    prefs_ = mock_system_state_.mock_prefs();
  }

  void QuitMainLoop();
  static gboolean StaticQuitMainLoop(gpointer data);

  void UpdateTestStart();
  void UpdateTestVerify();
  void RollbackTestStart(bool enterprise_rollback, bool stable_channel);
  void RollbackTestVerify();
  static gboolean StaticUpdateTestStart(gpointer data);
  static gboolean StaticUpdateTestVerify(gpointer data);
  static gboolean StaticRollbackTestStart(gpointer data);
  static gboolean StaticEnterpriseRollbackTestStart(gpointer data);
  static gboolean StaticStableChannelRollbackTestStart(gpointer data);
  static gboolean StaticRollbackTestVerify(gpointer data);

  void PingOmahaTestStart();
  static gboolean StaticPingOmahaTestStart(gpointer data);

  void ReadChannelFromPolicyTestStart();
  static gboolean StaticReadChannelFromPolicyTestStart(gpointer data);

  void ReadUpdateDisabledFromPolicyTestStart();
  static gboolean StaticReadUpdateDisabledFromPolicyTestStart(gpointer data);

  void ReadTargetVersionPrefixFromPolicyTestStart();
  static gboolean StaticReadTargetVersionPrefixFromPolicyTestStart(
      gpointer data);

  void ReadScatterFactorFromPolicyTestStart();
  static gboolean StaticReadScatterFactorFromPolicyTestStart(
      gpointer data);

  void DecrementUpdateCheckCountTestStart();
  static gboolean StaticDecrementUpdateCheckCountTestStart(
      gpointer data);

  void NoScatteringDoneDuringManualUpdateTestStart();
  static gboolean StaticNoScatteringDoneDuringManualUpdateTestStart(
      gpointer data);

  NiceMock<MockSystemState> mock_system_state_;
  NiceMock<MockDbusGlib> dbus_;
  UpdateAttempterUnderTest attempter_;
  NiceMock<ActionProcessorMock>* processor_;
  NiceMock<PrefsMock>* prefs_; // shortcut to mock_system_state_->mock_prefs()
  NiceMock<MockConnectionManager> mock_connection_manager;
  GMainLoop* loop_;
};

TEST_F(UpdateAttempterTest, ActionCompletedDownloadTest) {
  scoped_ptr<MockHttpFetcher> fetcher(new MockHttpFetcher("", 0, NULL));
  fetcher->FailTransfer(503);  // Sets the HTTP response code.
  DownloadAction action(prefs_, NULL, fetcher.release());
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _)).Times(0);
  attempter_.ActionCompleted(NULL, &action, kErrorCodeSuccess);
  EXPECT_EQ(503, attempter_.http_response_code());
  EXPECT_EQ(UPDATE_STATUS_FINALIZING, attempter_.status());
  ASSERT_TRUE(attempter_.error_event_.get() == NULL);
}

TEST_F(UpdateAttempterTest, ActionCompletedErrorTest) {
  ActionMock action;
  EXPECT_CALL(action, Type()).WillRepeatedly(Return("ActionMock"));
  attempter_.status_ = UPDATE_STATUS_DOWNLOADING;
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false));
  attempter_.ActionCompleted(NULL, &action, kErrorCodeError);
  ASSERT_TRUE(attempter_.error_event_.get() != NULL);
}

TEST_F(UpdateAttempterTest, ActionCompletedOmahaRequestTest) {
  scoped_ptr<MockHttpFetcher> fetcher(new MockHttpFetcher("", 0, NULL));
  fetcher->FailTransfer(500);  // Sets the HTTP response code.
  OmahaRequestAction action(&mock_system_state_, NULL,
                            fetcher.release(), false);
  ObjectCollectorAction<OmahaResponse> collector_action;
  BondActions(&action, &collector_action);
  OmahaResponse response;
  response.poll_interval = 234;
  action.SetOutputObject(response);
  UpdateCheckScheduler scheduler(&attempter_, &mock_system_state_);
  attempter_.set_update_check_scheduler(&scheduler);
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _)).Times(0);
  attempter_.ActionCompleted(NULL, &action, kErrorCodeSuccess);
  EXPECT_EQ(500, attempter_.http_response_code());
  EXPECT_EQ(UPDATE_STATUS_IDLE, attempter_.status());
  EXPECT_EQ(234, scheduler.poll_interval());
  ASSERT_TRUE(attempter_.error_event_.get() == NULL);
}

TEST_F(UpdateAttempterTest, RunAsRootConstructWithUpdatedMarkerTest) {
  extern const char* kUpdateCompletedMarker;
  const FilePath kMarker(kUpdateCompletedMarker);
  EXPECT_EQ(0, file_util::WriteFile(kMarker, "", 0));
  UpdateAttempterUnderTest attempter(&mock_system_state_, &dbus_);
  EXPECT_EQ(UPDATE_STATUS_UPDATED_NEED_REBOOT, attempter.status());
  EXPECT_TRUE(file_util::Delete(kMarker, false));
}

TEST_F(UpdateAttempterTest, GetErrorCodeForActionTest) {
  extern ErrorCode GetErrorCodeForAction(AbstractAction* action,
                                              ErrorCode code);
  EXPECT_EQ(kErrorCodeSuccess,
            GetErrorCodeForAction(NULL, kErrorCodeSuccess));

  MockSystemState mock_system_state;
  OmahaRequestAction omaha_request_action(&mock_system_state, NULL,
                                          NULL, false);
  EXPECT_EQ(kErrorCodeOmahaRequestError,
            GetErrorCodeForAction(&omaha_request_action, kErrorCodeError));
  OmahaResponseHandlerAction omaha_response_handler_action(&mock_system_state_);
  EXPECT_EQ(kErrorCodeOmahaResponseHandlerError,
            GetErrorCodeForAction(&omaha_response_handler_action,
                                  kErrorCodeError));
  FilesystemCopierAction filesystem_copier_action(
      &mock_system_state_, false, false);
  EXPECT_EQ(kErrorCodeFilesystemCopierError,
            GetErrorCodeForAction(&filesystem_copier_action, kErrorCodeError));
  PostinstallRunnerAction postinstall_runner_action;
  EXPECT_EQ(kErrorCodePostinstallRunnerError,
            GetErrorCodeForAction(&postinstall_runner_action,
                                  kErrorCodeError));
  ActionMock action_mock;
  EXPECT_CALL(action_mock, Type()).Times(1).WillOnce(Return("ActionMock"));
  EXPECT_EQ(kErrorCodeError,
            GetErrorCodeForAction(&action_mock, kErrorCodeError));
}

TEST_F(UpdateAttempterTest, DisableDeltaUpdateIfNeededTest) {
  attempter_.omaha_request_params_->set_delta_okay(true);
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_TRUE(attempter_.omaha_request_params_->delta_okay());
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures - 1),
          Return(true)));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_TRUE(attempter_.omaha_request_params_->delta_okay());
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures),
          Return(true)));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_FALSE(attempter_.omaha_request_params_->delta_okay());
  EXPECT_CALL(*prefs_, GetInt64(_, _)).Times(0);
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_FALSE(attempter_.omaha_request_params_->delta_okay());
}

TEST_F(UpdateAttempterTest, MarkDeltaUpdateFailureTest) {
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<1>(-1), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<1>(1), Return(true)))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures),
          Return(true)));
  EXPECT_CALL(*prefs_, SetInt64(Ne(kPrefsDeltaUpdateFailures), _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*prefs_, SetInt64(kPrefsDeltaUpdateFailures, 1)).Times(2);
  EXPECT_CALL(*prefs_, SetInt64(kPrefsDeltaUpdateFailures, 2)).Times(1);
  EXPECT_CALL(*prefs_, SetInt64(kPrefsDeltaUpdateFailures,
                               UpdateAttempter::kMaxDeltaUpdateFailures + 1))
      .Times(1);
  for (int i = 0; i < 4; i ++)
    attempter_.MarkDeltaUpdateFailure();
}

TEST_F(UpdateAttempterTest, ScheduleErrorEventActionNoEventTest) {
  EXPECT_CALL(*processor_, EnqueueAction(_)).Times(0);
  EXPECT_CALL(*processor_, StartProcessing()).Times(0);
  EXPECT_CALL(*mock_system_state_.mock_payload_state(), UpdateFailed(_))
      .Times(0);
  OmahaResponse response;
  string url1 = "http://url1";
  response.payload_urls.push_back(url1);
  response.payload_urls.push_back("https://url");
  EXPECT_CALL(*(mock_system_state_.mock_payload_state()), GetCurrentUrl())
      .WillRepeatedly(Return(url1));
  mock_system_state_.mock_payload_state()->SetResponse(response);
  attempter_.ScheduleErrorEventAction();
  EXPECT_EQ(url1, mock_system_state_.mock_payload_state()->GetCurrentUrl());
}

TEST_F(UpdateAttempterTest, ScheduleErrorEventActionTest) {
  EXPECT_CALL(*processor_,
              EnqueueAction(Property(&AbstractAction::Type,
                                     OmahaRequestAction::StaticType())))
      .Times(1);
  EXPECT_CALL(*processor_, StartProcessing()).Times(1);
  ErrorCode err = kErrorCodeError;
  EXPECT_CALL(*mock_system_state_.mock_payload_state(), UpdateFailed(err));
  attempter_.error_event_.reset(new OmahaEvent(OmahaEvent::kTypeUpdateComplete,
                                               OmahaEvent::kResultError,
                                               err));
  attempter_.ScheduleErrorEventAction();
  EXPECT_EQ(UPDATE_STATUS_REPORTING_ERROR_EVENT, attempter_.status());
}

TEST_F(UpdateAttempterTest, UpdateStatusToStringTest) {
  extern const char* UpdateStatusToString(UpdateStatus);
  EXPECT_STREQ("UPDATE_STATUS_IDLE", UpdateStatusToString(UPDATE_STATUS_IDLE));
  EXPECT_STREQ("UPDATE_STATUS_CHECKING_FOR_UPDATE",
               UpdateStatusToString(UPDATE_STATUS_CHECKING_FOR_UPDATE));
  EXPECT_STREQ("UPDATE_STATUS_UPDATE_AVAILABLE",
               UpdateStatusToString(UPDATE_STATUS_UPDATE_AVAILABLE));
  EXPECT_STREQ("UPDATE_STATUS_DOWNLOADING",
               UpdateStatusToString(UPDATE_STATUS_DOWNLOADING));
  EXPECT_STREQ("UPDATE_STATUS_VERIFYING",
               UpdateStatusToString(UPDATE_STATUS_VERIFYING));
  EXPECT_STREQ("UPDATE_STATUS_FINALIZING",
               UpdateStatusToString(UPDATE_STATUS_FINALIZING));
  EXPECT_STREQ("UPDATE_STATUS_UPDATED_NEED_REBOOT",
               UpdateStatusToString(UPDATE_STATUS_UPDATED_NEED_REBOOT));
  EXPECT_STREQ("UPDATE_STATUS_REPORTING_ERROR_EVENT",
               UpdateStatusToString(UPDATE_STATUS_REPORTING_ERROR_EVENT));
  EXPECT_STREQ("unknown status",
               UpdateStatusToString(static_cast<UpdateStatus>(-1)));
}

void UpdateAttempterTest::QuitMainLoop() {
  g_main_loop_quit(loop_);
}

gboolean UpdateAttempterTest::StaticQuitMainLoop(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->QuitMainLoop();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticUpdateTestStart(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->UpdateTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticUpdateTestVerify(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->UpdateTestVerify();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticRollbackTestStart(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->RollbackTestStart(false, false);
  return FALSE;
}

gboolean UpdateAttempterTest::StaticEnterpriseRollbackTestStart(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->RollbackTestStart(true, false);
  return FALSE;
}

gboolean UpdateAttempterTest::StaticStableChannelRollbackTestStart(
    gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->RollbackTestStart(
      false, true);
  return FALSE;
}

gboolean UpdateAttempterTest::StaticRollbackTestVerify(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->RollbackTestVerify();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticPingOmahaTestStart(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->PingOmahaTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticReadChannelFromPolicyTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->ReadChannelFromPolicyTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticReadUpdateDisabledFromPolicyTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->ReadUpdateDisabledFromPolicyTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticReadTargetVersionPrefixFromPolicyTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->ReadTargetVersionPrefixFromPolicyTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticReadScatterFactorFromPolicyTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->ReadScatterFactorFromPolicyTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticDecrementUpdateCheckCountTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->DecrementUpdateCheckCountTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticNoScatteringDoneDuringManualUpdateTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->NoScatteringDoneDuringManualUpdateTestStart();
  return FALSE;
}

namespace {
// Actions that will be built as part of an update check.
const string kUpdateActionTypes[] = {
  OmahaRequestAction::StaticType(),
  OmahaResponseHandlerAction::StaticType(),
  FilesystemCopierAction::StaticType(),
  FilesystemCopierAction::StaticType(),
  OmahaRequestAction::StaticType(),
  DownloadAction::StaticType(),
  OmahaRequestAction::StaticType(),
  FilesystemCopierAction::StaticType(),
  FilesystemCopierAction::StaticType(),
  PostinstallRunnerAction::StaticType(),
  OmahaRequestAction::StaticType()
};

// Actions that will be built as part of a user-initiated rollback.
const string kRollbackActionTypes[] = {
  InstallPlanAction::StaticType(),
  PostinstallRunnerAction::StaticType(),
};

}  // namespace {}

void UpdateAttempterTest::UpdateTestStart() {
  attempter_.set_http_response_code(200);
  InSequence s;
  for (size_t i = 0; i < arraysize(kUpdateActionTypes); ++i) {
    EXPECT_CALL(*processor_,
                EnqueueAction(Property(&AbstractAction::Type,
                                       kUpdateActionTypes[i]))).Times(1);
  }
  EXPECT_CALL(*processor_, StartProcessing()).Times(1);

  attempter_.Update("", "", false, false, false);
  g_idle_add(&StaticUpdateTestVerify, this);
}

void UpdateAttempterTest::UpdateTestVerify() {
  EXPECT_EQ(0, attempter_.http_response_code());
  EXPECT_EQ(&attempter_, processor_->delegate());
  EXPECT_EQ(arraysize(kUpdateActionTypes), attempter_.actions_.size());
  for (size_t i = 0; i < arraysize(kUpdateActionTypes); ++i) {
    EXPECT_EQ(kUpdateActionTypes[i], attempter_.actions_[i]->Type());
  }
  EXPECT_EQ(attempter_.response_handler_action_.get(),
            attempter_.actions_[1].get());
  DownloadAction* download_action =
      dynamic_cast<DownloadAction*>(attempter_.actions_[5].get());
  ASSERT_TRUE(download_action != NULL);
  EXPECT_EQ(&attempter_, download_action->delegate());
  EXPECT_EQ(UPDATE_STATUS_CHECKING_FOR_UPDATE, attempter_.status());
  g_main_loop_quit(loop_);
}

void UpdateAttempterTest::RollbackTestStart(
    bool enterprise_rollback, bool stable_channel) {
  // Create a device policy so that we can change settings.
  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_system_state_, device_policy()).WillRepeatedly(
      Return(device_policy));

  string install_path = "/dev/sda3";
  bool is_rollback_allowed = false;

  // We only allow rollback on devices that are neither enterprise enrolled or
  // not on the stable channel.
  if (!(enterprise_rollback || stable_channel)) {
     is_rollback_allowed = true;
  }

  // Set up the policy for the test given our args.
  if (stable_channel) {
    attempter_.omaha_request_params_->set_current_channel(
        string("stable-channel"));
  } else if (enterprise_rollback) {
    // We return an empty owner as this is an enterprise.
    EXPECT_CALL(*device_policy, GetOwner(_)).WillOnce(
        DoAll(SetArgumentPointee<0>(std::string("")),
        Return(true)));
  } else {
    // We return a fake owner as this is an owned consumer device.
    EXPECT_CALL(*device_policy, GetOwner(_)).WillOnce(
        DoAll(SetArgumentPointee<0>(std::string("fake.mail@fake.com")),
        Return(true)));
  }

  if (is_rollback_allowed) {
    InSequence s;
    for (size_t i = 0; i < arraysize(kRollbackActionTypes); ++i) {
      EXPECT_CALL(*processor_,
                  EnqueueAction(Property(&AbstractAction::Type,
                                         kRollbackActionTypes[i]))).Times(1);
    }
    EXPECT_CALL(*processor_, StartProcessing()).Times(1);

    EXPECT_TRUE(attempter_.Rollback(true, &install_path));
    g_idle_add(&StaticRollbackTestVerify, this);
  } else {
    EXPECT_FALSE(attempter_.Rollback(true, &install_path));
    g_main_loop_quit(loop_);
  }
}

void UpdateAttempterTest::RollbackTestVerify() {
  // Verifies the actions that were enqueued.
  EXPECT_EQ(&attempter_, processor_->delegate());
  EXPECT_EQ(arraysize(kRollbackActionTypes), attempter_.actions_.size());
  for (size_t i = 0; i < arraysize(kRollbackActionTypes); ++i) {
    EXPECT_EQ(kRollbackActionTypes[i], attempter_.actions_[i]->Type());
  }
  EXPECT_EQ(UPDATE_STATUS_ATTEMPTING_ROLLBACK, attempter_.status());
  InstallPlanAction* install_plan_action =
        dynamic_cast<InstallPlanAction*>(attempter_.actions_[0].get());
  InstallPlan* install_plan = install_plan_action->install_plan();
  EXPECT_EQ(install_plan->install_path, string("/dev/sda3"));
  EXPECT_EQ(install_plan->kernel_install_path, string("/dev/sda2"));
  EXPECT_EQ(install_plan->powerwash_required, true);
  g_main_loop_quit(loop_);
}

TEST_F(UpdateAttempterTest, UpdateTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticUpdateTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

TEST_F(UpdateAttempterTest, RollbackTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticRollbackTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

TEST_F(UpdateAttempterTest, StableChannelRollbackTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticStableChannelRollbackTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

TEST_F(UpdateAttempterTest, EnterpriseRollbackTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticEnterpriseRollbackTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::PingOmahaTestStart() {
  EXPECT_CALL(*processor_,
              EnqueueAction(Property(&AbstractAction::Type,
                                     OmahaRequestAction::StaticType())))
      .Times(1);
  EXPECT_CALL(*processor_, StartProcessing()).Times(1);
  attempter_.PingOmaha();
  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, PingOmahaTest) {
  UpdateCheckScheduler scheduler(&attempter_, &mock_system_state_);
  scheduler.enabled_ = true;
  EXPECT_FALSE(scheduler.scheduled_);
  attempter_.set_update_check_scheduler(&scheduler);
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticPingOmahaTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
  EXPECT_EQ(UPDATE_STATUS_UPDATED_NEED_REBOOT, attempter_.status());
  EXPECT_EQ(true, scheduler.scheduled_);
}

TEST_F(UpdateAttempterTest, CreatePendingErrorEventTest) {
  ActionMock action;
  const ErrorCode kCode = kErrorCodeDownloadTransferError;
  attempter_.CreatePendingErrorEvent(&action, kCode);
  ASSERT_TRUE(attempter_.error_event_.get() != NULL);
  EXPECT_EQ(OmahaEvent::kTypeUpdateComplete, attempter_.error_event_->type);
  EXPECT_EQ(OmahaEvent::kResultError, attempter_.error_event_->result);
  EXPECT_EQ(kCode | kErrorCodeTestOmahaUrlFlag,
            attempter_.error_event_->error_code);
}

TEST_F(UpdateAttempterTest, CreatePendingErrorEventResumedTest) {
  OmahaResponseHandlerAction *response_action =
      new OmahaResponseHandlerAction(&mock_system_state_);
  response_action->install_plan_.is_resume = true;
  attempter_.response_handler_action_.reset(response_action);
  ActionMock action;
  const ErrorCode kCode = kErrorCodeInstallDeviceOpenError;
  attempter_.CreatePendingErrorEvent(&action, kCode);
  ASSERT_TRUE(attempter_.error_event_.get() != NULL);
  EXPECT_EQ(OmahaEvent::kTypeUpdateComplete, attempter_.error_event_->type);
  EXPECT_EQ(OmahaEvent::kResultError, attempter_.error_event_->result);
  EXPECT_EQ(kCode | kErrorCodeResumedFlag | kErrorCodeTestOmahaUrlFlag,
            attempter_.error_event_->error_code);
}

TEST_F(UpdateAttempterTest, ReadChannelFromPolicy) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticReadChannelFromPolicyTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::ReadChannelFromPolicyTestStart() {
  // Tests that the update channel (aka release channel) is properly fetched
  // from the device policy.

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_system_state_, device_policy()).WillRepeatedly(
      Return(device_policy));

  EXPECT_CALL(*device_policy, GetReleaseChannelDelegated(_)).WillRepeatedly(
      DoAll(SetArgumentPointee<0>(bool(false)),
      Return(true)));

  EXPECT_CALL(*device_policy, GetReleaseChannel(_)).WillRepeatedly(
      DoAll(SetArgumentPointee<0>(std::string("beta-channel")),
      Return(true)));

  attempter_.omaha_request_params_->set_root("./UpdateAttempterTest");
  attempter_.Update("", "", false, false, false);
  EXPECT_EQ("beta-channel",
            attempter_.omaha_request_params_->target_channel());

  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, ReadUpdateDisabledFromPolicy) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticReadUpdateDisabledFromPolicyTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::ReadUpdateDisabledFromPolicyTestStart() {
  // Tests that the update_disbled flag is properly fetched
  // from the device policy.

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_system_state_, device_policy()).WillRepeatedly(
      Return(device_policy));

  EXPECT_CALL(*device_policy, GetUpdateDisabled(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(true),
          Return(true)));

  attempter_.Update("", "", false, false, false);
  EXPECT_TRUE(attempter_.omaha_request_params_->update_disabled());

  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, ReadTargetVersionPrefixFromPolicy) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticReadTargetVersionPrefixFromPolicyTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::ReadTargetVersionPrefixFromPolicyTestStart() {
  // Tests that the target_version_prefix value is properly fetched
  // from the device policy.

  const std::string target_version_prefix = "1412.";

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_system_state_, device_policy()).WillRepeatedly(
      Return(device_policy));

  EXPECT_CALL(*device_policy, GetTargetVersionPrefix(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(target_version_prefix),
          Return(true)));

  attempter_.Update("", "", false, false, false);
  EXPECT_EQ(target_version_prefix.c_str(),
            attempter_.omaha_request_params_->target_version_prefix());

  g_idle_add(&StaticQuitMainLoop, this);
}


TEST_F(UpdateAttempterTest, ReadScatterFactorFromPolicy) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticReadScatterFactorFromPolicyTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

// Tests that the scatter_factor_in_seconds value is properly fetched
// from the device policy.
void UpdateAttempterTest::ReadScatterFactorFromPolicyTestStart() {
  int64 scatter_factor_in_seconds = 36000;

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_system_state_, device_policy()).WillRepeatedly(
      Return(device_policy));

  EXPECT_CALL(*device_policy, GetScatterFactorInSeconds(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(scatter_factor_in_seconds),
          Return(true)));

  attempter_.Update("", "", false, false, false);
  EXPECT_EQ(scatter_factor_in_seconds, attempter_.scatter_factor_.InSeconds());

  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, DecrementUpdateCheckCountTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticDecrementUpdateCheckCountTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::DecrementUpdateCheckCountTestStart() {
  // Tests that the scatter_factor_in_seconds value is properly fetched
  // from the device policy and is decremented if value > 0.
  int64 initial_value = 5;
  Prefs prefs;
  attempter_.prefs_ = &prefs;

  EXPECT_CALL(mock_system_state_,
              IsOOBEComplete()).WillRepeatedly(Return(true));

  string prefs_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("/tmp/ue_ut_prefs.XXXXXX",
                                       &prefs_dir));
  ScopedDirRemover temp_dir_remover(prefs_dir);

  LOG_IF(ERROR, !prefs.Init(FilePath(prefs_dir)))
      << "Failed to initialize preferences.";
  EXPECT_TRUE(prefs.SetInt64(kPrefsUpdateCheckCount, initial_value));

  int64 scatter_factor_in_seconds = 10;

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_system_state_, device_policy()).WillRepeatedly(
      Return(device_policy));

  EXPECT_CALL(*device_policy, GetScatterFactorInSeconds(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(scatter_factor_in_seconds),
          Return(true)));

  attempter_.Update("", "", false, false, false);
  EXPECT_EQ(scatter_factor_in_seconds, attempter_.scatter_factor_.InSeconds());

  // Make sure the file still exists.
  EXPECT_TRUE(prefs.Exists(kPrefsUpdateCheckCount));

  int64 new_value;
  EXPECT_TRUE(prefs.GetInt64(kPrefsUpdateCheckCount, &new_value));
  EXPECT_EQ(initial_value - 1, new_value);

  EXPECT_TRUE(
      attempter_.omaha_request_params_->update_check_count_wait_enabled());

  // However, if the count is already 0, it's not decremented. Test that.
  initial_value = 0;
  EXPECT_TRUE(prefs.SetInt64(kPrefsUpdateCheckCount, initial_value));
  attempter_.Update("", "", false, false, false);
  EXPECT_TRUE(prefs.Exists(kPrefsUpdateCheckCount));
  EXPECT_TRUE(prefs.GetInt64(kPrefsUpdateCheckCount, &new_value));
  EXPECT_EQ(initial_value, new_value);

  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, NoScatteringDoneDuringManualUpdateTestStart) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticNoScatteringDoneDuringManualUpdateTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::NoScatteringDoneDuringManualUpdateTestStart() {
  // Tests that no scattering logic is enabled if the update check
  // is manually done (as opposed to a scheduled update check)
  int64 initial_value = 8;
  Prefs prefs;
  attempter_.prefs_ = &prefs;

  EXPECT_CALL(mock_system_state_,
              IsOOBEComplete()).WillRepeatedly(Return(true));

  string prefs_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("/tmp/ue_ut_prefs.XXXXXX",
                                       &prefs_dir));
  ScopedDirRemover temp_dir_remover(prefs_dir);

  LOG_IF(ERROR, !prefs.Init(FilePath(prefs_dir)))
      << "Failed to initialize preferences.";
  EXPECT_TRUE(prefs.SetInt64(kPrefsWallClockWaitPeriod, initial_value));
  EXPECT_TRUE(prefs.SetInt64(kPrefsUpdateCheckCount, initial_value));

  // make sure scatter_factor is non-zero as scattering is disabled
  // otherwise.
  int64 scatter_factor_in_seconds = 50;

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_system_state_, device_policy()).WillRepeatedly(
      Return(device_policy));

  EXPECT_CALL(*device_policy, GetScatterFactorInSeconds(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(scatter_factor_in_seconds),
          Return(true)));

  // Trigger an interactive check so we can test that scattering is disabled.
  attempter_.Update("", "", false, true, false);
  EXPECT_EQ(scatter_factor_in_seconds, attempter_.scatter_factor_.InSeconds());

  // Make sure scattering is disabled for manual (i.e. user initiated) update
  // checks and all artifacts are removed.
  EXPECT_FALSE(
      attempter_.omaha_request_params_->wall_clock_based_wait_enabled());
  EXPECT_FALSE(prefs.Exists(kPrefsWallClockWaitPeriod));
  EXPECT_EQ(0, attempter_.omaha_request_params_->waiting_period().InSeconds());
  EXPECT_FALSE(
      attempter_.omaha_request_params_->update_check_count_wait_enabled());
  EXPECT_FALSE(prefs.Exists(kPrefsUpdateCheckCount));

  g_idle_add(&StaticQuitMainLoop, this);
}

}  // namespace chromeos_update_engine
