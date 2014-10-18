// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include <base/files/file_util.h>
#include <gtest/gtest.h>
#include <policy/libpolicy.h>
#include <policy/mock_device_policy.h>

#include "update_engine/action_mock.h"
#include "update_engine/action_processor_mock.h"
#include "update_engine/fake_clock.h"
#include "update_engine/fake_system_state.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/install_plan.h"
#include "update_engine/mock_dbus_wrapper.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/mock_p2p_manager.h"
#include "update_engine/mock_payload_state.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/prefs.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_attempter.h"
#include "update_engine/utils.h"

using base::Time;
using base::TimeDelta;
using std::string;
using std::unique_ptr;
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
  // We always feed an explicit update completed marker name; however, unless
  // explicitly specified, we feed an empty string, which causes the
  // UpdateAttempter class to ignore / not write the marker file.
  UpdateAttempterUnderTest(SystemState* system_state,
                           DBusWrapperInterface* dbus_iface)
      : UpdateAttempterUnderTest(system_state, dbus_iface, "") {}

  UpdateAttempterUnderTest(SystemState* system_state,
                           DBusWrapperInterface* dbus_iface,
                           const std::string& update_completed_marker)
      : UpdateAttempter(system_state, dbus_iface, update_completed_marker) {}

  // Wrap the update scheduling method, allowing us to opt out of scheduled
  // updates for testing purposes.
  void ScheduleUpdates() override {
    schedule_updates_called_ = true;
    if (do_schedule_updates_) {
      UpdateAttempter::ScheduleUpdates();
    } else {
      LOG(INFO) << "[TEST] Update scheduling disabled.";
    }
  }
  void EnableScheduleUpdates() { do_schedule_updates_ = true; }
  void DisableScheduleUpdates() { do_schedule_updates_ = false; }

  // Indicates whether ScheduleUpdates() was called.
  bool schedule_updates_called() const { return schedule_updates_called_; }

 private:
  bool schedule_updates_called_ = false;
  bool do_schedule_updates_ = true;
};

class UpdateAttempterTest : public ::testing::Test {
 protected:
  UpdateAttempterTest()
      : attempter_(&fake_system_state_, &dbus_),
        mock_connection_manager(&fake_system_state_),
        loop_(nullptr) {
    // Override system state members.
    fake_system_state_.set_connection_manager(&mock_connection_manager);
    fake_system_state_.set_update_attempter(&attempter_);

    // Finish initializing the attempter.
    attempter_.Init();

    // We set the set_good_kernel command to a non-existent path so it fails to
    // run it. This avoids the async call to the command and continues the
    // update process right away. Tests testing that behavior can override the
    // default set_good_kernel command if needed.
    attempter_.set_good_kernel_cmd_ = "/path/to/non-existent/command";
  }

  virtual void SetUp() {
    CHECK(utils::MakeTempDirectory("UpdateAttempterTest-XXXXXX", &test_dir_));

    EXPECT_EQ(nullptr, attempter_.dbus_service_);
    EXPECT_NE(nullptr, attempter_.system_state_);
    EXPECT_EQ(0, attempter_.http_response_code_);
    EXPECT_EQ(utils::kCpuSharesNormal, attempter_.shares_);
    EXPECT_EQ(nullptr, attempter_.manage_shares_source_);
    EXPECT_FALSE(attempter_.download_active_);
    EXPECT_EQ(UPDATE_STATUS_IDLE, attempter_.status_);
    EXPECT_EQ(0.0, attempter_.download_progress_);
    EXPECT_EQ(0, attempter_.last_checked_time_);
    EXPECT_EQ("0.0.0.0", attempter_.new_version_);
    EXPECT_EQ(0, attempter_.new_payload_size_);
    processor_ = new NiceMock<ActionProcessorMock>();
    attempter_.processor_.reset(processor_);  // Transfers ownership.
    prefs_ = fake_system_state_.mock_prefs();
  }

  virtual void TearDown() {
    utils::RecursiveUnlinkDir(test_dir_);
  }

  void QuitMainLoop();
  static gboolean StaticQuitMainLoop(gpointer data);

  void UpdateTestStart();
  void UpdateTestVerify();
  void RollbackTestStart(bool enterprise_rollback,
                         bool valid_slot);
  void RollbackTestVerify();
  static gboolean StaticUpdateTestStart(gpointer data);
  static gboolean StaticUpdateTestVerify(gpointer data);
  static gboolean StaticRollbackTestStart(gpointer data);
  static gboolean StaticInvalidSlotRollbackTestStart(gpointer data);
  static gboolean StaticEnterpriseRollbackTestStart(gpointer data);
  static gboolean StaticRollbackTestVerify(gpointer data);

  void PingOmahaTestStart();
  static gboolean StaticPingOmahaTestStart(gpointer data);

  void ReadScatterFactorFromPolicyTestStart();
  static gboolean StaticReadScatterFactorFromPolicyTestStart(
      gpointer data);

  void DecrementUpdateCheckCountTestStart();
  static gboolean StaticDecrementUpdateCheckCountTestStart(
      gpointer data);

  void NoScatteringDoneDuringManualUpdateTestStart();
  static gboolean StaticNoScatteringDoneDuringManualUpdateTestStart(
      gpointer data);

  void P2PNotEnabledStart();
  static gboolean StaticP2PNotEnabled(gpointer data);

  void P2PEnabledStart();
  static gboolean StaticP2PEnabled(gpointer data);

  void P2PEnabledInteractiveStart();
  static gboolean StaticP2PEnabledInteractive(gpointer data);

  void P2PEnabledStartingFailsStart();
  static gboolean StaticP2PEnabledStartingFails(gpointer data);

  void P2PEnabledHousekeepingFailsStart();
  static gboolean StaticP2PEnabledHousekeepingFails(gpointer data);

  FakeSystemState fake_system_state_;
  NiceMock<MockDBusWrapper> dbus_;
  UpdateAttempterUnderTest attempter_;
  NiceMock<ActionProcessorMock>* processor_;
  NiceMock<PrefsMock>* prefs_;  // shortcut to fake_system_state_->mock_prefs()
  NiceMock<MockConnectionManager> mock_connection_manager;
  GMainLoop* loop_;

  string test_dir_;
};

TEST_F(UpdateAttempterTest, ActionCompletedDownloadTest) {
  unique_ptr<MockHttpFetcher> fetcher(new MockHttpFetcher("", 0, nullptr));
  fetcher->FailTransfer(503);  // Sets the HTTP response code.
  DownloadAction action(prefs_, nullptr, fetcher.release());
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _)).Times(0);
  attempter_.ActionCompleted(nullptr, &action, ErrorCode::kSuccess);
  EXPECT_EQ(503, attempter_.http_response_code());
  EXPECT_EQ(UPDATE_STATUS_FINALIZING, attempter_.status());
  ASSERT_EQ(nullptr, attempter_.error_event_.get());
}

TEST_F(UpdateAttempterTest, ActionCompletedErrorTest) {
  ActionMock action;
  EXPECT_CALL(action, Type()).WillRepeatedly(Return("ActionMock"));
  attempter_.status_ = UPDATE_STATUS_DOWNLOADING;
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false));
  attempter_.ActionCompleted(nullptr, &action, ErrorCode::kError);
  ASSERT_NE(nullptr, attempter_.error_event_.get());
}

TEST_F(UpdateAttempterTest, ActionCompletedOmahaRequestTest) {
  unique_ptr<MockHttpFetcher> fetcher(new MockHttpFetcher("", 0, nullptr));
  fetcher->FailTransfer(500);  // Sets the HTTP response code.
  OmahaRequestAction action(&fake_system_state_, nullptr,
                            fetcher.release(), false);
  ObjectCollectorAction<OmahaResponse> collector_action;
  BondActions(&action, &collector_action);
  OmahaResponse response;
  response.poll_interval = 234;
  action.SetOutputObject(response);
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _)).Times(0);
  attempter_.ActionCompleted(nullptr, &action, ErrorCode::kSuccess);
  EXPECT_EQ(500, attempter_.http_response_code());
  EXPECT_EQ(UPDATE_STATUS_IDLE, attempter_.status());
  EXPECT_EQ(234, attempter_.server_dictated_poll_interval_);
  ASSERT_TRUE(attempter_.error_event_.get() == nullptr);
}

TEST_F(UpdateAttempterTest, RunAsRootConstructWithUpdatedMarkerTest) {
  string test_update_completed_marker;
  CHECK(utils::MakeTempFile(
          "update_attempter_unittest-update_completed_marker-XXXXXX",
          &test_update_completed_marker, nullptr));
  ScopedPathUnlinker completed_marker_unlinker(test_update_completed_marker);
  const base::FilePath marker(test_update_completed_marker);
  EXPECT_EQ(0, base::WriteFile(marker, "", 0));
  UpdateAttempterUnderTest attempter(&fake_system_state_, &dbus_,
                                     test_update_completed_marker);
  EXPECT_EQ(UPDATE_STATUS_UPDATED_NEED_REBOOT, attempter.status());
}

TEST_F(UpdateAttempterTest, GetErrorCodeForActionTest) {
  extern ErrorCode GetErrorCodeForAction(AbstractAction* action,
                                              ErrorCode code);
  EXPECT_EQ(ErrorCode::kSuccess,
            GetErrorCodeForAction(nullptr, ErrorCode::kSuccess));

  FakeSystemState fake_system_state;
  OmahaRequestAction omaha_request_action(&fake_system_state, nullptr,
                                          nullptr, false);
  EXPECT_EQ(ErrorCode::kOmahaRequestError,
            GetErrorCodeForAction(&omaha_request_action, ErrorCode::kError));
  OmahaResponseHandlerAction omaha_response_handler_action(&fake_system_state_);
  EXPECT_EQ(ErrorCode::kOmahaResponseHandlerError,
            GetErrorCodeForAction(&omaha_response_handler_action,
                                  ErrorCode::kError));
  FilesystemCopierAction filesystem_copier_action(
      &fake_system_state_, false, false);
  EXPECT_EQ(ErrorCode::kFilesystemCopierError,
            GetErrorCodeForAction(&filesystem_copier_action,
                                  ErrorCode::kError));
  PostinstallRunnerAction postinstall_runner_action;
  EXPECT_EQ(ErrorCode::kPostinstallRunnerError,
            GetErrorCodeForAction(&postinstall_runner_action,
                                  ErrorCode::kError));
  ActionMock action_mock;
  EXPECT_CALL(action_mock, Type()).Times(1).WillOnce(Return("ActionMock"));
  EXPECT_EQ(ErrorCode::kError,
            GetErrorCodeForAction(&action_mock, ErrorCode::kError));
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
  EXPECT_CALL(*fake_system_state_.mock_payload_state(), UpdateFailed(_))
      .Times(0);
  OmahaResponse response;
  string url1 = "http://url1";
  response.payload_urls.push_back(url1);
  response.payload_urls.push_back("https://url");
  EXPECT_CALL(*(fake_system_state_.mock_payload_state()), GetCurrentUrl())
      .WillRepeatedly(Return(url1));
  fake_system_state_.mock_payload_state()->SetResponse(response);
  attempter_.ScheduleErrorEventAction();
  EXPECT_EQ(url1, fake_system_state_.mock_payload_state()->GetCurrentUrl());
}

TEST_F(UpdateAttempterTest, ScheduleErrorEventActionTest) {
  EXPECT_CALL(*processor_,
              EnqueueAction(Property(&AbstractAction::Type,
                                     OmahaRequestAction::StaticType())))
      .Times(1);
  EXPECT_CALL(*processor_, StartProcessing()).Times(1);
  ErrorCode err = ErrorCode::kError;
  EXPECT_CALL(*fake_system_state_.mock_payload_state(), UpdateFailed(err));
  attempter_.error_event_.reset(new OmahaEvent(OmahaEvent::kTypeUpdateComplete,
                                               OmahaEvent::kResultError,
                                               err));
  attempter_.ScheduleErrorEventAction();
  EXPECT_EQ(UPDATE_STATUS_REPORTING_ERROR_EVENT, attempter_.status());
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
  reinterpret_cast<UpdateAttempterTest*>(data)->RollbackTestStart(
      false, true);
  return FALSE;
}

gboolean UpdateAttempterTest::StaticInvalidSlotRollbackTestStart(
    gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->RollbackTestStart(
      false, false);
  return FALSE;
}

gboolean UpdateAttempterTest::StaticEnterpriseRollbackTestStart(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->RollbackTestStart(
      true, true);
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
const string kUpdateActionTypes[] = {  // NOLINT(runtime/string)
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
const string kRollbackActionTypes[] = {  // NOLINT(runtime/string)
  InstallPlanAction::StaticType(),
  PostinstallRunnerAction::StaticType(),
};

}  // namespace

void UpdateAttempterTest::UpdateTestStart() {
  attempter_.set_http_response_code(200);
  InSequence s;
  for (size_t i = 0; i < arraysize(kUpdateActionTypes); ++i) {
    EXPECT_CALL(*processor_,
                EnqueueAction(Property(&AbstractAction::Type,
                                       kUpdateActionTypes[i]))).Times(1);
  }
  EXPECT_CALL(*processor_, StartProcessing()).Times(1);

  attempter_.Update("", "", "", "", false, false);
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
  ASSERT_NE(nullptr, download_action);
  EXPECT_EQ(&attempter_, download_action->delegate());
  EXPECT_EQ(UPDATE_STATUS_CHECKING_FOR_UPDATE, attempter_.status());
  g_main_loop_quit(loop_);
}

void UpdateAttempterTest::RollbackTestStart(
    bool enterprise_rollback, bool valid_slot) {
  // Create a device policy so that we can change settings.
  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  fake_system_state_.set_device_policy(device_policy);

  if (!valid_slot) {
    // References bootable kernels in fake_hardware.h
    string rollback_kernel = "/dev/sdz2";
    LOG(INFO) << "Test Mark Unbootable: " << rollback_kernel;
    fake_system_state_.fake_hardware()->MarkKernelUnbootable(
        rollback_kernel);
  }

  bool is_rollback_allowed = false;

  // We only allow rollback on devices that are not enterprise enrolled and
  // which have a valid slot to rollback to.
  if (!enterprise_rollback && valid_slot) {
     is_rollback_allowed = true;
  }

  if (enterprise_rollback) {
    // We return an empty owner as this is an enterprise.
    EXPECT_CALL(*device_policy, GetOwner(_)).WillRepeatedly(
        DoAll(SetArgumentPointee<0>(std::string("")),
        Return(true)));
  } else {
    // We return a fake owner as this is an owned consumer device.
    EXPECT_CALL(*device_policy, GetOwner(_)).WillRepeatedly(
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

    EXPECT_TRUE(attempter_.Rollback(true));
    g_idle_add(&StaticRollbackTestVerify, this);
  } else {
    EXPECT_FALSE(attempter_.Rollback(true));
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
  // Matches fake_hardware.h -> rollback should move from kernel/boot device
  // pair to other pair.
  EXPECT_EQ(install_plan->install_path, string("/dev/sdz3"));
  EXPECT_EQ(install_plan->kernel_install_path, string("/dev/sdz2"));
  EXPECT_EQ(install_plan->powerwash_required, true);
  g_main_loop_quit(loop_);
}

TEST_F(UpdateAttempterTest, UpdateTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticUpdateTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
}

TEST_F(UpdateAttempterTest, RollbackTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticRollbackTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
}

TEST_F(UpdateAttempterTest, InvalidSlotRollbackTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticInvalidSlotRollbackTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
}

TEST_F(UpdateAttempterTest, EnterpriseRollbackTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticEnterpriseRollbackTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
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
  EXPECT_FALSE(attempter_.waiting_for_scheduled_check_);
  EXPECT_FALSE(attempter_.schedule_updates_called());
  // Disable scheduling of subsequnet checks; we're using the DefaultPolicy in
  // testing, which is more permissive than we want to handle here.
  attempter_.DisableScheduleUpdates();
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticPingOmahaTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
  EXPECT_EQ(UPDATE_STATUS_UPDATED_NEED_REBOOT, attempter_.status());
  EXPECT_TRUE(attempter_.schedule_updates_called());
}

TEST_F(UpdateAttempterTest, CreatePendingErrorEventTest) {
  ActionMock action;
  const ErrorCode kCode = ErrorCode::kDownloadTransferError;
  attempter_.CreatePendingErrorEvent(&action, kCode);
  ASSERT_NE(nullptr, attempter_.error_event_.get());
  EXPECT_EQ(OmahaEvent::kTypeUpdateComplete, attempter_.error_event_->type);
  EXPECT_EQ(OmahaEvent::kResultError, attempter_.error_event_->result);
  EXPECT_EQ(
      static_cast<ErrorCode>(static_cast<int>(kCode) |
                             static_cast<int>(ErrorCode::kTestOmahaUrlFlag)),
      attempter_.error_event_->error_code);
}

TEST_F(UpdateAttempterTest, CreatePendingErrorEventResumedTest) {
  OmahaResponseHandlerAction *response_action =
      new OmahaResponseHandlerAction(&fake_system_state_);
  response_action->install_plan_.is_resume = true;
  attempter_.response_handler_action_.reset(response_action);
  ActionMock action;
  const ErrorCode kCode = ErrorCode::kInstallDeviceOpenError;
  attempter_.CreatePendingErrorEvent(&action, kCode);
  ASSERT_NE(nullptr, attempter_.error_event_.get());
  EXPECT_EQ(OmahaEvent::kTypeUpdateComplete, attempter_.error_event_->type);
  EXPECT_EQ(OmahaEvent::kResultError, attempter_.error_event_->result);
  EXPECT_EQ(
      static_cast<ErrorCode>(
          static_cast<int>(kCode) |
          static_cast<int>(ErrorCode::kResumedFlag) |
          static_cast<int>(ErrorCode::kTestOmahaUrlFlag)),
      attempter_.error_event_->error_code);
}

TEST_F(UpdateAttempterTest, P2PNotStartedAtStartupWhenNotEnabled) {
  MockP2PManager mock_p2p_manager;
  fake_system_state_.set_p2p_manager(&mock_p2p_manager);
  mock_p2p_manager.fake().SetP2PEnabled(false);
  EXPECT_CALL(mock_p2p_manager, EnsureP2PRunning()).Times(0);
  attempter_.UpdateEngineStarted();
}

TEST_F(UpdateAttempterTest, P2PNotStartedAtStartupWhenEnabledButNotSharing) {
  MockP2PManager mock_p2p_manager;
  fake_system_state_.set_p2p_manager(&mock_p2p_manager);
  mock_p2p_manager.fake().SetP2PEnabled(true);
  EXPECT_CALL(mock_p2p_manager, EnsureP2PRunning()).Times(0);
  attempter_.UpdateEngineStarted();
}

TEST_F(UpdateAttempterTest, P2PStartedAtStartupWhenEnabledAndSharing) {
  MockP2PManager mock_p2p_manager;
  fake_system_state_.set_p2p_manager(&mock_p2p_manager);
  mock_p2p_manager.fake().SetP2PEnabled(true);
  mock_p2p_manager.fake().SetCountSharedFilesResult(1);
  EXPECT_CALL(mock_p2p_manager, EnsureP2PRunning()).Times(1);
  attempter_.UpdateEngineStarted();
}

TEST_F(UpdateAttempterTest, P2PNotEnabled) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticP2PNotEnabled, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
}
gboolean UpdateAttempterTest::StaticP2PNotEnabled(gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->P2PNotEnabledStart();
  return FALSE;
}
void UpdateAttempterTest::P2PNotEnabledStart() {
  // If P2P is not enabled, check that we do not attempt housekeeping
  // and do not convey that p2p is to be used.
  MockP2PManager mock_p2p_manager;
  fake_system_state_.set_p2p_manager(&mock_p2p_manager);
  mock_p2p_manager.fake().SetP2PEnabled(false);
  EXPECT_CALL(mock_p2p_manager, PerformHousekeeping()).Times(0);
  attempter_.Update("", "", "", "", false, false);
  EXPECT_FALSE(attempter_.omaha_request_params_->use_p2p_for_downloading());
  EXPECT_FALSE(attempter_.omaha_request_params_->use_p2p_for_sharing());
  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, P2PEnabledStartingFails) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticP2PEnabledStartingFails, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
}
gboolean UpdateAttempterTest::StaticP2PEnabledStartingFails(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->P2PEnabledStartingFailsStart();
  return FALSE;
}
void UpdateAttempterTest::P2PEnabledStartingFailsStart() {
  // If p2p is enabled, but starting it fails ensure we don't do
  // any housekeeping and do not convey that p2p should be used.
  MockP2PManager mock_p2p_manager;
  fake_system_state_.set_p2p_manager(&mock_p2p_manager);
  mock_p2p_manager.fake().SetP2PEnabled(true);
  mock_p2p_manager.fake().SetEnsureP2PRunningResult(false);
  mock_p2p_manager.fake().SetPerformHousekeepingResult(false);
  EXPECT_CALL(mock_p2p_manager, PerformHousekeeping()).Times(0);
  attempter_.Update("", "", "", "", false, false);
  EXPECT_FALSE(attempter_.omaha_request_params_->use_p2p_for_downloading());
  EXPECT_FALSE(attempter_.omaha_request_params_->use_p2p_for_sharing());
  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, P2PEnabledHousekeepingFails) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticP2PEnabledHousekeepingFails, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
}
gboolean UpdateAttempterTest::StaticP2PEnabledHousekeepingFails(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->P2PEnabledHousekeepingFailsStart();
  return FALSE;
}
void UpdateAttempterTest::P2PEnabledHousekeepingFailsStart() {
  // If p2p is enabled, starting it works but housekeeping fails, ensure
  // we do not convey p2p is to be used.
  MockP2PManager mock_p2p_manager;
  fake_system_state_.set_p2p_manager(&mock_p2p_manager);
  mock_p2p_manager.fake().SetP2PEnabled(true);
  mock_p2p_manager.fake().SetEnsureP2PRunningResult(true);
  mock_p2p_manager.fake().SetPerformHousekeepingResult(false);
  EXPECT_CALL(mock_p2p_manager, PerformHousekeeping()).Times(1);
  attempter_.Update("", "", "", "", false, false);
  EXPECT_FALSE(attempter_.omaha_request_params_->use_p2p_for_downloading());
  EXPECT_FALSE(attempter_.omaha_request_params_->use_p2p_for_sharing());
  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, P2PEnabled) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticP2PEnabled, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
}
gboolean UpdateAttempterTest::StaticP2PEnabled(gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->P2PEnabledStart();
  return FALSE;
}
void UpdateAttempterTest::P2PEnabledStart() {
  MockP2PManager mock_p2p_manager;
  fake_system_state_.set_p2p_manager(&mock_p2p_manager);
  // If P2P is enabled and starting it works, check that we performed
  // housekeeping and that we convey p2p should be used.
  mock_p2p_manager.fake().SetP2PEnabled(true);
  mock_p2p_manager.fake().SetEnsureP2PRunningResult(true);
  mock_p2p_manager.fake().SetPerformHousekeepingResult(true);
  EXPECT_CALL(mock_p2p_manager, PerformHousekeeping()).Times(1);
  attempter_.Update("", "", "", "", false, false);
  EXPECT_TRUE(attempter_.omaha_request_params_->use_p2p_for_downloading());
  EXPECT_TRUE(attempter_.omaha_request_params_->use_p2p_for_sharing());
  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, P2PEnabledInteractive) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticP2PEnabledInteractive, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
}
gboolean UpdateAttempterTest::StaticP2PEnabledInteractive(gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->P2PEnabledInteractiveStart();
  return FALSE;
}
void UpdateAttempterTest::P2PEnabledInteractiveStart() {
  MockP2PManager mock_p2p_manager;
  fake_system_state_.set_p2p_manager(&mock_p2p_manager);
  // For an interactive check, if P2P is enabled and starting it
  // works, check that we performed housekeeping and that we convey
  // p2p should be used for sharing but NOT for downloading.
  mock_p2p_manager.fake().SetP2PEnabled(true);
  mock_p2p_manager.fake().SetEnsureP2PRunningResult(true);
  mock_p2p_manager.fake().SetPerformHousekeepingResult(true);
  EXPECT_CALL(mock_p2p_manager, PerformHousekeeping()).Times(1);
  attempter_.Update("", "", "", "", false, true /* interactive */);
  EXPECT_FALSE(attempter_.omaha_request_params_->use_p2p_for_downloading());
  EXPECT_TRUE(attempter_.omaha_request_params_->use_p2p_for_sharing());
  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, ReadScatterFactorFromPolicy) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticReadScatterFactorFromPolicyTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
}

// Tests that the scatter_factor_in_seconds value is properly fetched
// from the device policy.
void UpdateAttempterTest::ReadScatterFactorFromPolicyTestStart() {
  int64_t scatter_factor_in_seconds = 36000;

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  fake_system_state_.set_device_policy(device_policy);

  EXPECT_CALL(*device_policy, GetScatterFactorInSeconds(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(scatter_factor_in_seconds),
          Return(true)));

  attempter_.Update("", "", "", "", false, false);
  EXPECT_EQ(scatter_factor_in_seconds, attempter_.scatter_factor_.InSeconds());

  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, DecrementUpdateCheckCountTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticDecrementUpdateCheckCountTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = nullptr;
}

void UpdateAttempterTest::DecrementUpdateCheckCountTestStart() {
  // Tests that the scatter_factor_in_seconds value is properly fetched
  // from the device policy and is decremented if value > 0.
  int64_t initial_value = 5;
  Prefs prefs;
  attempter_.prefs_ = &prefs;

  fake_system_state_.fake_hardware()->SetIsOOBEComplete(Time::UnixEpoch());

  string prefs_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("ue_ut_prefs.XXXXXX",
                                       &prefs_dir));
  ScopedDirRemover temp_dir_remover(prefs_dir);

  LOG_IF(ERROR, !prefs.Init(base::FilePath(prefs_dir)))
      << "Failed to initialize preferences.";
  EXPECT_TRUE(prefs.SetInt64(kPrefsUpdateCheckCount, initial_value));

  int64_t scatter_factor_in_seconds = 10;

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  fake_system_state_.set_device_policy(device_policy);

  EXPECT_CALL(*device_policy, GetScatterFactorInSeconds(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(scatter_factor_in_seconds),
          Return(true)));

  attempter_.Update("", "", "", "", false, false);
  EXPECT_EQ(scatter_factor_in_seconds, attempter_.scatter_factor_.InSeconds());

  // Make sure the file still exists.
  EXPECT_TRUE(prefs.Exists(kPrefsUpdateCheckCount));

  int64_t new_value;
  EXPECT_TRUE(prefs.GetInt64(kPrefsUpdateCheckCount, &new_value));
  EXPECT_EQ(initial_value - 1, new_value);

  EXPECT_TRUE(
      attempter_.omaha_request_params_->update_check_count_wait_enabled());

  // However, if the count is already 0, it's not decremented. Test that.
  initial_value = 0;
  EXPECT_TRUE(prefs.SetInt64(kPrefsUpdateCheckCount, initial_value));
  attempter_.Update("", "", "", "", false, false);
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
  loop_ = nullptr;
}

void UpdateAttempterTest::NoScatteringDoneDuringManualUpdateTestStart() {
  // Tests that no scattering logic is enabled if the update check
  // is manually done (as opposed to a scheduled update check)
  int64_t initial_value = 8;
  Prefs prefs;
  attempter_.prefs_ = &prefs;

  fake_system_state_.fake_hardware()->SetIsOOBEComplete(Time::UnixEpoch());

  string prefs_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("ue_ut_prefs.XXXXXX",
                                       &prefs_dir));
  ScopedDirRemover temp_dir_remover(prefs_dir);

  LOG_IF(ERROR, !prefs.Init(base::FilePath(prefs_dir)))
      << "Failed to initialize preferences.";
  EXPECT_TRUE(prefs.SetInt64(kPrefsWallClockWaitPeriod, initial_value));
  EXPECT_TRUE(prefs.SetInt64(kPrefsUpdateCheckCount, initial_value));

  // make sure scatter_factor is non-zero as scattering is disabled
  // otherwise.
  int64_t scatter_factor_in_seconds = 50;

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  fake_system_state_.set_device_policy(device_policy);

  EXPECT_CALL(*device_policy, GetScatterFactorInSeconds(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(scatter_factor_in_seconds),
          Return(true)));

  // Trigger an interactive check so we can test that scattering is disabled.
  attempter_.Update("", "", "", "", false, true);
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

// Checks that we only report daily metrics at most every 24 hours.
TEST_F(UpdateAttempterTest, ReportDailyMetrics) {
  FakeClock fake_clock;
  Prefs prefs;
  string temp_dir;

  // We need persistent preferences for this test
  EXPECT_TRUE(utils::MakeTempDirectory("UpdateAttempterTest.XXXXXX",
                                       &temp_dir));
  prefs.Init(base::FilePath(temp_dir));
  fake_system_state_.set_clock(&fake_clock);
  fake_system_state_.set_prefs(&prefs);

  Time epoch = Time::FromInternalValue(0);
  fake_clock.SetWallclockTime(epoch);

  // If there is no kPrefsDailyMetricsLastReportedAt state variable,
  // we should report.
  EXPECT_TRUE(attempter_.CheckAndReportDailyMetrics());
  // We should not report again if no time has passed.
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());

  // We should not report if only 10 hours has passed.
  fake_clock.SetWallclockTime(epoch + TimeDelta::FromHours(10));
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());

  // We should not report if only 24 hours - 1 sec has passed.
  fake_clock.SetWallclockTime(epoch + TimeDelta::FromHours(24) -
                              TimeDelta::FromSeconds(1));
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());

  // We should report if 24 hours has passed.
  fake_clock.SetWallclockTime(epoch + TimeDelta::FromHours(24));
  EXPECT_TRUE(attempter_.CheckAndReportDailyMetrics());

  // But then we should not report again..
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());

  // .. until another 24 hours has passed
  fake_clock.SetWallclockTime(epoch + TimeDelta::FromHours(47));
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());
  fake_clock.SetWallclockTime(epoch + TimeDelta::FromHours(48));
  EXPECT_TRUE(attempter_.CheckAndReportDailyMetrics());
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());

  // .. and another 24 hours
  fake_clock.SetWallclockTime(epoch + TimeDelta::FromHours(71));
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());
  fake_clock.SetWallclockTime(epoch + TimeDelta::FromHours(72));
  EXPECT_TRUE(attempter_.CheckAndReportDailyMetrics());
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());

  // If the span between time of reporting and present time is
  // negative, we report. This is in order to reset the timestamp and
  // avoid an edge condition whereby a distant point in the future is
  // in the state variable resulting in us never ever reporting again.
  fake_clock.SetWallclockTime(epoch + TimeDelta::FromHours(71));
  EXPECT_TRUE(attempter_.CheckAndReportDailyMetrics());
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());

  // In this case we should not update until the clock reads 71 + 24 = 95.
  // Check that.
  fake_clock.SetWallclockTime(epoch + TimeDelta::FromHours(94));
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());
  fake_clock.SetWallclockTime(epoch + TimeDelta::FromHours(95));
  EXPECT_TRUE(attempter_.CheckAndReportDailyMetrics());
  EXPECT_FALSE(attempter_.CheckAndReportDailyMetrics());

  EXPECT_TRUE(utils::RecursiveUnlinkDir(temp_dir));
}

TEST_F(UpdateAttempterTest, BootTimeInUpdateMarkerFile) {
  const string update_completed_marker = test_dir_ + "/update-completed-marker";
  UpdateAttempterUnderTest attempter(&fake_system_state_, &dbus_,
                                     update_completed_marker);

  FakeClock fake_clock;
  fake_clock.SetBootTime(Time::FromTimeT(42));
  fake_system_state_.set_clock(&fake_clock);

  Time boot_time;
  EXPECT_FALSE(attempter.GetBootTimeAtUpdate(&boot_time));

  attempter.WriteUpdateCompletedMarker();

  EXPECT_TRUE(attempter.GetBootTimeAtUpdate(&boot_time));
  EXPECT_EQ(boot_time.ToTimeT(), 42);
}

}  // namespace chromeos_update_engine
