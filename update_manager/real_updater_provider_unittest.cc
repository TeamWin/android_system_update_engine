// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/real_updater_provider.h"

#include <memory>
#include <string>

#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/fake_system_state.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/update_attempter_mock.h"
#include "update_engine/update_manager/umtest_utils.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::FakeClock;
using chromeos_update_engine::FakeSystemState;
using chromeos_update_engine::OmahaRequestParams;
using chromeos_update_engine::PrefsMock;
using chromeos_update_engine::UpdateAttempterMock;
using std::string;
using std::unique_ptr;
using testing::Return;
using testing::SetArgPointee;
using testing::StrEq;
using testing::_;

namespace {

// Generates a fixed timestamp for use in faking the current time.
Time FixedTime() {
  Time::Exploded now_exp;
  now_exp.year = 2014;
  now_exp.month = 3;
  now_exp.day_of_week = 2;
  now_exp.day_of_month = 18;
  now_exp.hour = 8;
  now_exp.minute = 5;
  now_exp.second = 33;
  now_exp.millisecond = 675;
  return Time::FromLocalExploded(now_exp);
}

// Rounds down a timestamp to the nearest second. This is useful when faking
// times that are converted to time_t (no sub-second resolution).
Time RoundedToSecond(Time time) {
  Time::Exploded exp;
  time.LocalExplode(&exp);
  exp.millisecond = 0;
  return Time::FromLocalExploded(exp);
}

}  // namespace

namespace chromeos_update_manager {

class UmRealUpdaterProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    fake_clock_ = fake_sys_state_.fake_clock();
    provider_.reset(new RealUpdaterProvider(&fake_sys_state_));
    ASSERT_NE(nullptr, provider_.get());
    // Check that provider initializes correctly.
    ASSERT_TRUE(provider_->Init());
  }

  // Sets up mock expectations for testing a variable that reads a Boolean pref
  // |key|. |key_exists| determines whether the key is present. If it is, then
  // |get_boolean_success| determines whether reading it is successful, and if
  // so |output| is the value being read.
  void SetupReadBooleanPref(const char* key, bool key_exists,
                            bool get_boolean_success, bool output) {
    PrefsMock* const mock_prefs = fake_sys_state_.mock_prefs();
    EXPECT_CALL(*mock_prefs, Exists(StrEq(key))).WillOnce(Return(key_exists));
    if (key_exists) {
      auto& get_boolean = EXPECT_CALL(
          *fake_sys_state_.mock_prefs(), GetBoolean(StrEq(key), _));
      if (get_boolean_success)
        get_boolean.WillOnce(DoAll(SetArgPointee<1>(output), Return(true)));
      else
        get_boolean.WillOnce(Return(false));
    }
  }

  // Sets up mock expectations for testing the update completed time reporting.
  // |valid| determines whether the returned time is valid. Returns the expected
  // update completed time value.
  Time SetupUpdateCompletedTime(bool valid) {
    const TimeDelta kDurationSinceUpdate = TimeDelta::FromMinutes(7);
    const Time kUpdateBootTime = Time() + kDurationSinceUpdate * 2;
    const Time kCurrBootTime = (valid ?
                                kUpdateBootTime + kDurationSinceUpdate :
                                kUpdateBootTime - kDurationSinceUpdate);
    const Time kCurrWallclockTime = FixedTime();
    EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
                GetBootTimeAtUpdate(_))
        .WillOnce(DoAll(SetArgPointee<0>(kUpdateBootTime), Return(true)));
    fake_clock_->SetBootTime(kCurrBootTime);
    fake_clock_->SetWallclockTime(kCurrWallclockTime);
    return kCurrWallclockTime - kDurationSinceUpdate;
  }

  FakeSystemState fake_sys_state_;
  FakeClock* fake_clock_;  // Short for fake_sys_state_.fake_clock()
  unique_ptr<RealUpdaterProvider> provider_;
};

TEST_F(UmRealUpdaterProviderTest, UpdaterStartedTimeIsWallclockTime) {
  fake_clock_->SetWallclockTime(Time::FromDoubleT(123.456));
  fake_clock_->SetMonotonicTime(Time::FromDoubleT(456.123));
  // Run SetUp again to re-setup the provider under test to use these values.
  SetUp();
  UmTestUtils::ExpectVariableHasValue(Time::FromDoubleT(123.456),
                                      provider_->var_updater_started_time());
}

TEST_F(UmRealUpdaterProviderTest, GetLastCheckedTimeOkay) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(FixedTime().ToTimeT()), Return(true)));
  UmTestUtils::ExpectVariableHasValue(RoundedToSecond(FixedTime()),
                                      provider_->var_last_checked_time());
}

TEST_F(UmRealUpdaterProviderTest, GetLastCheckedTimeFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(Return(false));
  UmTestUtils::ExpectVariableNotSet(provider_->var_last_checked_time());
}

TEST_F(UmRealUpdaterProviderTest, GetProgressOkayMin) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(0.0), Return(true)));
  UmTestUtils::ExpectVariableHasValue(0.0, provider_->var_progress());
}

TEST_F(UmRealUpdaterProviderTest, GetProgressOkayMid) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(0.3), Return(true)));
  UmTestUtils::ExpectVariableHasValue(0.3, provider_->var_progress());
}

TEST_F(UmRealUpdaterProviderTest, GetProgressOkayMax) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(1.0), Return(true)));
  UmTestUtils::ExpectVariableHasValue(1.0, provider_->var_progress());
}

TEST_F(UmRealUpdaterProviderTest, GetProgressFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(Return(false));
  UmTestUtils::ExpectVariableNotSet(provider_->var_progress());
}

TEST_F(UmRealUpdaterProviderTest, GetProgressFailTooSmall) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(-2.0), Return(true)));
  UmTestUtils::ExpectVariableNotSet(provider_->var_progress());
}

TEST_F(UmRealUpdaterProviderTest, GetProgressFailTooBig) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(2.0), Return(true)));
  UmTestUtils::ExpectVariableNotSet(provider_->var_progress());
}

TEST_F(UmRealUpdaterProviderTest, GetStageOkayIdle) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(update_engine::kUpdateStatusIdle),
                      Return(true)));
  UmTestUtils::ExpectVariableHasValue(Stage::kIdle, provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageOkayCheckingForUpdate) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<2>(update_engine::kUpdateStatusCheckingForUpdate),
              Return(true)));
  UmTestUtils::ExpectVariableHasValue(Stage::kCheckingForUpdate,
                                      provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageOkayUpdateAvailable) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<2>(update_engine::kUpdateStatusUpdateAvailable),
              Return(true)));
  UmTestUtils::ExpectVariableHasValue(Stage::kUpdateAvailable,
                                      provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageOkayDownloading) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(update_engine::kUpdateStatusDownloading),
                      Return(true)));
  UmTestUtils::ExpectVariableHasValue(Stage::kDownloading,
                                      provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageOkayVerifying) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(update_engine::kUpdateStatusVerifying),
                      Return(true)));
  UmTestUtils::ExpectVariableHasValue(Stage::kVerifying,
                                      provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageOkayFinalizing) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(update_engine::kUpdateStatusFinalizing),
                      Return(true)));
  UmTestUtils::ExpectVariableHasValue(Stage::kFinalizing,
                                      provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageOkayUpdatedNeedReboot) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<2>(update_engine::kUpdateStatusUpdatedNeedReboot),
              Return(true)));
  UmTestUtils::ExpectVariableHasValue(Stage::kUpdatedNeedReboot,
                                      provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageOkayReportingErrorEvent) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<2>(update_engine::kUpdateStatusReportingErrorEvent),
              Return(true)));
  UmTestUtils::ExpectVariableHasValue(Stage::kReportingErrorEvent,
                                      provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageOkayAttemptingRollback) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<2>(update_engine::kUpdateStatusAttemptingRollback),
              Return(true)));
  UmTestUtils::ExpectVariableHasValue(Stage::kAttemptingRollback,
                                      provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(Return(false));
  UmTestUtils::ExpectVariableNotSet(provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageFailUnknown) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>("FooUpdateEngineState"),
                      Return(true)));
  UmTestUtils::ExpectVariableNotSet(provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetStageFailEmpty) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(""), Return(true)));
  UmTestUtils::ExpectVariableNotSet(provider_->var_stage());
}

TEST_F(UmRealUpdaterProviderTest, GetNewVersionOkay) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>("1.2.0"), Return(true)));
  UmTestUtils::ExpectVariableHasValue(string("1.2.0"),
                                      provider_->var_new_version());
}

TEST_F(UmRealUpdaterProviderTest, GetNewVersionFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(Return(false));
  UmTestUtils::ExpectVariableNotSet(provider_->var_new_version());
}

TEST_F(UmRealUpdaterProviderTest, GetPayloadSizeOkayZero) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(static_cast<int64_t>(0)), Return(true)));
  UmTestUtils::ExpectVariableHasValue(static_cast<int64_t>(0),
                                      provider_->var_payload_size());
}

TEST_F(UmRealUpdaterProviderTest, GetPayloadSizeOkayArbitrary) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(static_cast<int64_t>(567890)),
                      Return(true)));
  UmTestUtils::ExpectVariableHasValue(static_cast<int64_t>(567890),
                                      provider_->var_payload_size());
}

TEST_F(UmRealUpdaterProviderTest, GetPayloadSizeOkayTwoGigabytes) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(static_cast<int64_t>(1) << 31),
                      Return(true)));
  UmTestUtils::ExpectVariableHasValue(static_cast<int64_t>(1) << 31,
                                      provider_->var_payload_size());
}

TEST_F(UmRealUpdaterProviderTest, GetPayloadSizeFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(Return(false));
  UmTestUtils::ExpectVariableNotSet(provider_->var_payload_size());
}

TEST_F(UmRealUpdaterProviderTest, GetPayloadSizeFailNegative) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(static_cast<int64_t>(-1024)),
                      Return(true)));
  UmTestUtils::ExpectVariableNotSet(provider_->var_payload_size());
}

TEST_F(UmRealUpdaterProviderTest, GetCurrChannelOkay) {
  const string kChannelName("foo-channel");
  OmahaRequestParams request_params(&fake_sys_state_);
  request_params.Init("", "", false);
  request_params.set_current_channel(kChannelName);
  fake_sys_state_.set_request_params(&request_params);
  UmTestUtils::ExpectVariableHasValue(kChannelName,
                                      provider_->var_curr_channel());
}

TEST_F(UmRealUpdaterProviderTest, GetCurrChannelFailEmpty) {
  OmahaRequestParams request_params(&fake_sys_state_);
  request_params.Init("", "", false);
  request_params.set_current_channel("");
  fake_sys_state_.set_request_params(&request_params);
  UmTestUtils::ExpectVariableNotSet(provider_->var_curr_channel());
}

TEST_F(UmRealUpdaterProviderTest, GetNewChannelOkay) {
  const string kChannelName("foo-channel");
  OmahaRequestParams request_params(&fake_sys_state_);
  request_params.Init("", "", false);
  request_params.set_target_channel(kChannelName);
  fake_sys_state_.set_request_params(&request_params);
  UmTestUtils::ExpectVariableHasValue(kChannelName,
                                      provider_->var_new_channel());
}

TEST_F(UmRealUpdaterProviderTest, GetNewChannelFailEmpty) {
  OmahaRequestParams request_params(&fake_sys_state_);
  request_params.Init("", "", false);
  request_params.set_target_channel("");
  fake_sys_state_.set_request_params(&request_params);
  UmTestUtils::ExpectVariableNotSet(provider_->var_new_channel());
}

TEST_F(UmRealUpdaterProviderTest, GetP2PEnabledOkayPrefDoesntExist) {
  SetupReadBooleanPref(chromeos_update_engine::kPrefsP2PEnabled,
                       false, false, false);
  UmTestUtils::ExpectVariableHasValue(false, provider_->var_p2p_enabled());
}

TEST_F(UmRealUpdaterProviderTest, GetP2PEnabledOkayPrefReadsFalse) {
  SetupReadBooleanPref(chromeos_update_engine::kPrefsP2PEnabled,
                       true, true, false);
  UmTestUtils::ExpectVariableHasValue(false, provider_->var_p2p_enabled());
}

TEST_F(UmRealUpdaterProviderTest, GetP2PEnabledOkayPrefReadsTrue) {
  SetupReadBooleanPref(chromeos_update_engine::kPrefsP2PEnabled,
                       true, true, true);
  UmTestUtils::ExpectVariableHasValue(true, provider_->var_p2p_enabled());
}

TEST_F(UmRealUpdaterProviderTest, GetP2PEnabledFailCannotReadPref) {
  SetupReadBooleanPref(chromeos_update_engine::kPrefsP2PEnabled,
                       true, false, false);
  UmTestUtils::ExpectVariableNotSet(provider_->var_p2p_enabled());
}

TEST_F(UmRealUpdaterProviderTest, GetCellularEnabledOkayPrefDoesntExist) {
  SetupReadBooleanPref(
      chromeos_update_engine::kPrefsUpdateOverCellularPermission,
      false, false, false);
  UmTestUtils::ExpectVariableHasValue(false, provider_->var_cellular_enabled());
}

TEST_F(UmRealUpdaterProviderTest, GetCellularEnabledOkayPrefReadsFalse) {
  SetupReadBooleanPref(
      chromeos_update_engine::kPrefsUpdateOverCellularPermission,
      true, true, false);
  UmTestUtils::ExpectVariableHasValue(false, provider_->var_cellular_enabled());
}

TEST_F(UmRealUpdaterProviderTest, GetCellularEnabledOkayPrefReadsTrue) {
  SetupReadBooleanPref(
      chromeos_update_engine::kPrefsUpdateOverCellularPermission,
      true, true, true);
  UmTestUtils::ExpectVariableHasValue(true, provider_->var_cellular_enabled());
}

TEST_F(UmRealUpdaterProviderTest, GetCellularEnabledFailCannotReadPref) {
  SetupReadBooleanPref(
      chromeos_update_engine::kPrefsUpdateOverCellularPermission,
      true, false, false);
  UmTestUtils::ExpectVariableNotSet(provider_->var_cellular_enabled());
}

TEST_F(UmRealUpdaterProviderTest, GetUpdateCompletedTimeOkay) {
  Time expected = SetupUpdateCompletedTime(true);
  UmTestUtils::ExpectVariableHasValue(expected,
                                      provider_->var_update_completed_time());
}

TEST_F(UmRealUpdaterProviderTest, GetUpdateCompletedTimeFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(), GetBootTimeAtUpdate(_))
      .WillOnce(Return(false));
  UmTestUtils::ExpectVariableNotSet(provider_->var_update_completed_time());
}

TEST_F(UmRealUpdaterProviderTest, GetUpdateCompletedTimeFailInvalidValue) {
  SetupUpdateCompletedTime(false);
  UmTestUtils::ExpectVariableNotSet(provider_->var_update_completed_time());
}

TEST_F(UmRealUpdaterProviderTest, GetConsecutiveFailedUpdateChecks) {
  const unsigned int kNumFailedChecks = 3;
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              consecutive_failed_update_checks())
      .WillRepeatedly(Return(kNumFailedChecks));
  UmTestUtils::ExpectVariableHasValue(
      kNumFailedChecks, provider_->var_consecutive_failed_update_checks());
}

TEST_F(UmRealUpdaterProviderTest, GetServerDictatedPollInterval) {
  const unsigned int kPollInterval = 2 * 60 * 60;  // Two hours.
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              server_dictated_poll_interval())
      .WillRepeatedly(Return(kPollInterval));
  UmTestUtils::ExpectVariableHasValue(
      kPollInterval, provider_->var_server_dictated_poll_interval());
}

}  // namespace chromeos_update_manager
