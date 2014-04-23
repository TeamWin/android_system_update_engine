// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_updater_provider.h"

#include <string>

#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/fake_system_state.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/update_attempter_mock.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::FakeClock;
using chromeos_update_engine::FakeSystemState;
using chromeos_update_engine::OmahaRequestParams;
using chromeos_update_engine::PrefsMock;
using chromeos_update_engine::UpdateAttempterMock;
using std::string;
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

namespace chromeos_policy_manager {

class PmRealUpdaterProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    fake_sys_state_.set_clock(&fake_clock_);
    provider_.reset(new RealUpdaterProvider(&fake_sys_state_));
    PMTEST_ASSERT_NOT_NULL(provider_.get());
    // Check that provider initializes corrrectly.
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
    fake_clock_.SetBootTime(kCurrBootTime);
    fake_clock_.SetWallclockTime(kCurrWallclockTime);
    return kCurrWallclockTime - kDurationSinceUpdate;
  }

  FakeSystemState fake_sys_state_;
  FakeClock fake_clock_;
  scoped_ptr<RealUpdaterProvider> provider_;
};

TEST_F(PmRealUpdaterProviderTest, GetLastCheckedTimeOkay) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(FixedTime().ToTimeT()), Return(true)));
  PmTestUtils::ExpectVariableHasValue(RoundedToSecond(FixedTime()),
                                      provider_->var_last_checked_time());
}

TEST_F(PmRealUpdaterProviderTest, GetLastCheckedTimeFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(Return(false));
  PmTestUtils::ExpectVariableNotSet(provider_->var_last_checked_time());
}

TEST_F(PmRealUpdaterProviderTest, GetProgressOkayMin) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(0.0), Return(true)));
  PmTestUtils::ExpectVariableHasValue(0.0, provider_->var_progress());
}

TEST_F(PmRealUpdaterProviderTest, GetProgressOkayMid) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(0.3), Return(true)));
  PmTestUtils::ExpectVariableHasValue(0.3, provider_->var_progress());
}

TEST_F(PmRealUpdaterProviderTest, GetProgressOkayMax) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(1.0), Return(true)));
  PmTestUtils::ExpectVariableHasValue(1.0, provider_->var_progress());
}

TEST_F(PmRealUpdaterProviderTest, GetProgressFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(Return(false));
  PmTestUtils::ExpectVariableNotSet(provider_->var_progress());
}

TEST_F(PmRealUpdaterProviderTest, GetProgressFailTooSmall) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(-2.0), Return(true)));
  PmTestUtils::ExpectVariableNotSet(provider_->var_progress());
}

TEST_F(PmRealUpdaterProviderTest, GetProgressFailTooBig) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(2.0), Return(true)));
  PmTestUtils::ExpectVariableNotSet(provider_->var_progress());
}

TEST_F(PmRealUpdaterProviderTest, GetStageOkayIdle) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(update_engine::kUpdateStatusIdle),
                      Return(true)));
  PmTestUtils::ExpectVariableHasValue(Stage::kIdle, provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageOkayCheckingForUpdate) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<2>(update_engine::kUpdateStatusCheckingForUpdate),
              Return(true)));
  PmTestUtils::ExpectVariableHasValue(Stage::kCheckingForUpdate,
                                      provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageOkayUpdateAvailable) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<2>(update_engine::kUpdateStatusUpdateAvailable),
              Return(true)));
  PmTestUtils::ExpectVariableHasValue(Stage::kUpdateAvailable,
                                      provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageOkayDownloading) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(update_engine::kUpdateStatusDownloading),
                      Return(true)));
  PmTestUtils::ExpectVariableHasValue(Stage::kDownloading,
                                      provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageOkayVerifying) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(update_engine::kUpdateStatusVerifying),
                      Return(true)));
  PmTestUtils::ExpectVariableHasValue(Stage::kVerifying,
                                      provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageOkayFinalizing) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(update_engine::kUpdateStatusFinalizing),
                      Return(true)));
  PmTestUtils::ExpectVariableHasValue(Stage::kFinalizing,
                                      provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageOkayUpdatedNeedReboot) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<2>(update_engine::kUpdateStatusUpdatedNeedReboot),
              Return(true)));
  PmTestUtils::ExpectVariableHasValue(Stage::kUpdatedNeedReboot,
                                      provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageOkayReportingErrorEvent) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<2>(update_engine::kUpdateStatusReportingErrorEvent),
              Return(true)));
  PmTestUtils::ExpectVariableHasValue(Stage::kReportingErrorEvent,
                                      provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageOkayAttemptingRollback) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<2>(update_engine::kUpdateStatusAttemptingRollback),
              Return(true)));
  PmTestUtils::ExpectVariableHasValue(Stage::kAttemptingRollback,
                                      provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(Return(false));
  PmTestUtils::ExpectVariableNotSet(provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageFailUnknown) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>("FooUpdateEngineState"),
                      Return(true)));
  PmTestUtils::ExpectVariableNotSet(provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetStageFailEmpty) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(""), Return(true)));
  PmTestUtils::ExpectVariableNotSet(provider_->var_stage());
}

TEST_F(PmRealUpdaterProviderTest, GetNewVersionOkay) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>("1.2.0"), Return(true)));
  PmTestUtils::ExpectVariableHasValue(string("1.2.0"),
                                      provider_->var_new_version());
}

TEST_F(PmRealUpdaterProviderTest, GetNewVersionFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(Return(false));
  PmTestUtils::ExpectVariableNotSet(provider_->var_new_version());
}

TEST_F(PmRealUpdaterProviderTest, GetPayloadSizeOkayZero) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(static_cast<int64_t>(0)), Return(true)));
  PmTestUtils::ExpectVariableHasValue(static_cast<size_t>(0),
                                      provider_->var_payload_size());
}

TEST_F(PmRealUpdaterProviderTest, GetPayloadSizeOkayArbitrary) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(static_cast<int64_t>(567890)),
                      Return(true)));
  PmTestUtils::ExpectVariableHasValue(static_cast<size_t>(567890),
                                      provider_->var_payload_size());
}

TEST_F(PmRealUpdaterProviderTest, GetPayloadSizeOkayTwoGigabytes) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(static_cast<int64_t>(1) << 31),
                      Return(true)));
  PmTestUtils::ExpectVariableHasValue(static_cast<size_t>(1) << 31,
                                      provider_->var_payload_size());
}

TEST_F(PmRealUpdaterProviderTest, GetPayloadSizeFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(Return(false));
  PmTestUtils::ExpectVariableNotSet(provider_->var_payload_size());
}

TEST_F(PmRealUpdaterProviderTest, GetPayloadSizeFailNegative) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(),
              GetStatus(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(static_cast<int64_t>(-1024)),
                      Return(true)));
  PmTestUtils::ExpectVariableNotSet(provider_->var_payload_size());
}

TEST_F(PmRealUpdaterProviderTest, GetCurrChannelOkay) {
  const string kChannelName("foo-channel");
  OmahaRequestParams request_params(&fake_sys_state_);
  request_params.Init("", "", false);
  request_params.set_current_channel(kChannelName);
  fake_sys_state_.set_request_params(&request_params);
  PmTestUtils::ExpectVariableHasValue(kChannelName,
                                      provider_->var_curr_channel());
}

TEST_F(PmRealUpdaterProviderTest, GetCurrChannelFailEmpty) {
  OmahaRequestParams request_params(&fake_sys_state_);
  request_params.Init("", "", false);
  request_params.set_current_channel("");
  fake_sys_state_.set_request_params(&request_params);
  PmTestUtils::ExpectVariableNotSet(provider_->var_curr_channel());
}

TEST_F(PmRealUpdaterProviderTest, GetNewChannelOkay) {
  const string kChannelName("foo-channel");
  OmahaRequestParams request_params(&fake_sys_state_);
  request_params.Init("", "", false);
  request_params.set_target_channel(kChannelName);
  fake_sys_state_.set_request_params(&request_params);
  PmTestUtils::ExpectVariableHasValue(kChannelName,
                                      provider_->var_new_channel());
}

TEST_F(PmRealUpdaterProviderTest, GetNewChannelFailEmpty) {
  OmahaRequestParams request_params(&fake_sys_state_);
  request_params.Init("", "", false);
  request_params.set_target_channel("");
  fake_sys_state_.set_request_params(&request_params);
  PmTestUtils::ExpectVariableNotSet(provider_->var_new_channel());
}

TEST_F(PmRealUpdaterProviderTest, GetP2PEnabledOkayPrefDoesntExist) {
  SetupReadBooleanPref(chromeos_update_engine::kPrefsP2PEnabled,
                       false, false, false);
  PmTestUtils::ExpectVariableHasValue(false, provider_->var_p2p_enabled());
}

TEST_F(PmRealUpdaterProviderTest, GetP2PEnabledOkayPrefReadsFalse) {
  SetupReadBooleanPref(chromeos_update_engine::kPrefsP2PEnabled,
                       true, true, false);
  PmTestUtils::ExpectVariableHasValue(false, provider_->var_p2p_enabled());
}

TEST_F(PmRealUpdaterProviderTest, GetP2PEnabledOkayPrefReadsTrue) {
  SetupReadBooleanPref(chromeos_update_engine::kPrefsP2PEnabled,
                       true, true, true);
  PmTestUtils::ExpectVariableHasValue(true, provider_->var_p2p_enabled());
}

TEST_F(PmRealUpdaterProviderTest, GetP2PEnabledFailCannotReadPref) {
  SetupReadBooleanPref(chromeos_update_engine::kPrefsP2PEnabled,
                       true, false, false);
  PmTestUtils::ExpectVariableNotSet(provider_->var_p2p_enabled());
}

TEST_F(PmRealUpdaterProviderTest, GetCellularEnabledOkayPrefDoesntExist) {
  SetupReadBooleanPref(
      chromeos_update_engine::kPrefsUpdateOverCellularPermission,
      false, false, false);
  PmTestUtils::ExpectVariableHasValue(false, provider_->var_cellular_enabled());
}

TEST_F(PmRealUpdaterProviderTest, GetCellularEnabledOkayPrefReadsFalse) {
  SetupReadBooleanPref(
      chromeos_update_engine::kPrefsUpdateOverCellularPermission,
      true, true, false);
  PmTestUtils::ExpectVariableHasValue(false, provider_->var_cellular_enabled());
}

TEST_F(PmRealUpdaterProviderTest, GetCellularEnabledOkayPrefReadsTrue) {
  SetupReadBooleanPref(
      chromeos_update_engine::kPrefsUpdateOverCellularPermission,
      true, true, true);
  PmTestUtils::ExpectVariableHasValue(true, provider_->var_cellular_enabled());
}

TEST_F(PmRealUpdaterProviderTest, GetCellularEnabledFailCannotReadPref) {
  SetupReadBooleanPref(
      chromeos_update_engine::kPrefsUpdateOverCellularPermission,
      true, false, false);
  PmTestUtils::ExpectVariableNotSet(provider_->var_cellular_enabled());
}

TEST_F(PmRealUpdaterProviderTest, GetUpdateCompletedTimeOkay) {
  Time expected = SetupUpdateCompletedTime(true);
  PmTestUtils::ExpectVariableHasValue(expected,
                                      provider_->var_update_completed_time());
}

TEST_F(PmRealUpdaterProviderTest, GetUpdateCompletedTimeFailNoValue) {
  EXPECT_CALL(*fake_sys_state_.mock_update_attempter(), GetBootTimeAtUpdate(_))
      .WillOnce(Return(false));
  PmTestUtils::ExpectVariableNotSet(provider_->var_update_completed_time());
}

TEST_F(PmRealUpdaterProviderTest, GetUpdateCompletedTimeFailInvalidValue) {
  SetupUpdateCompletedTime(false);
  PmTestUtils::ExpectVariableNotSet(provider_->var_update_completed_time());
}

}  // namespace chromeos_policy_manager
