// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/chromeos_policy.h"

#include <set>
#include <string>
#include <vector>

#include <base/time/time.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/update_manager/evaluation_context.h"
#include "update_engine/update_manager/fake_state.h"
#include "update_engine/update_manager/umtest_utils.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::ErrorCode;
using chromeos_update_engine::FakeClock;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_manager {

class UmChromeOSPolicyTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SetUpDefaultClock();
    eval_ctx_ = new EvaluationContext(&fake_clock_, TimeDelta::FromSeconds(5));
    SetUpDefaultState();
    SetUpDefaultDevicePolicy();
  }

  // Sets the clock to fixed values.
  void SetUpDefaultClock() {
    fake_clock_.SetMonotonicTime(Time::FromInternalValue(12345678L));
    fake_clock_.SetWallclockTime(Time::FromInternalValue(12345678901234L));
  }

  void SetUpDefaultState() {
    fake_state_.updater_provider()->var_updater_started_time()->reset(
        new Time(fake_clock_.GetWallclockTime()));
    fake_state_.updater_provider()->var_last_checked_time()->reset(
        new Time(fake_clock_.GetWallclockTime()));
    fake_state_.updater_provider()->var_consecutive_failed_update_checks()->
        reset(new unsigned int(0));  // NOLINT(readability/casting)

    fake_state_.random_provider()->var_seed()->reset(
        new uint64_t(4));  // chosen by fair dice roll.
                           // guaranteed to be random.

    // No device policy loaded by default.
    fake_state_.device_policy_provider()->var_device_policy_is_loaded()->reset(
        new bool(false));

    // For the purpose of the tests, this is an official build and OOBE was
    // completed.
    fake_state_.system_provider()->var_is_official_build()->reset(
        new bool(true));
    fake_state_.system_provider()->var_is_oobe_complete()->reset(
        new bool(true));
    fake_state_.system_provider()->var_is_boot_device_removable()->reset(
        new bool(false));

    // Connection is wifi, untethered.
    fake_state_.shill_provider()->var_conn_type()->
        reset(new ConnectionType(ConnectionType::kWifi));
    fake_state_.shill_provider()->var_conn_tethering()->
        reset(new ConnectionTethering(ConnectionTethering::kNotDetected));
  }

  // Sets up a default device policy that does not impose any restrictions
  // (HTTP) nor enables any features (P2P).
  void SetUpDefaultDevicePolicy() {
    fake_state_.device_policy_provider()->var_device_policy_is_loaded()->reset(
        new bool(true));
    fake_state_.device_policy_provider()->var_update_disabled()->reset(
        new bool(false));
    fake_state_.device_policy_provider()->
        var_allowed_connection_types_for_update()->reset(nullptr);
    fake_state_.device_policy_provider()->var_scatter_factor()->reset(
        new TimeDelta());
    fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
        new bool(true));
    fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
        new bool(false));
    fake_state_.device_policy_provider()->var_release_channel_delegated()->
        reset(new bool(true));
  }

  // Configures the UpdateCheckAllowed policy to return a desired value by
  // faking the current wall clock time as needed. Restores the default state.
  // This is used when testing policies that depend on this one.
  void SetUpdateCheckAllowed(bool allow_check) {
    Time next_update_check;
    ExpectPolicyStatus(EvalStatus::kSucceeded,
                       &ChromeOSPolicy::NextUpdateCheckTime,
                       &next_update_check);
    SetUpDefaultState();
    SetUpDefaultDevicePolicy();
    Time curr_time = next_update_check;
    if (allow_check)
      curr_time += TimeDelta::FromSeconds(1);
    else
      curr_time -= TimeDelta::FromSeconds(1);
    fake_clock_.SetWallclockTime(curr_time);
  }

  // Returns a default UpdateState structure: first seen time is calculated
  // backward from the current wall clock time, update was seen just once;
  // there's a single HTTP download URL with a maximum of 10 allowed failures;
  // there is no scattering wait period and the max allowed is 7 days, there is
  // no check threshold and none is allowed.
  UpdateState GetDefaultUpdateState(TimeDelta update_first_seen_period) {
    UpdateState update_state = {
      fake_clock_.GetWallclockTime() - update_first_seen_period, 1,
      vector<string>(1, "http://fake/url/"), 10, 0, 0, vector<ErrorCode>(),
      TimeDelta(), TimeDelta::FromDays(7), 0, 0, 0
    };
    return update_state;
  }

  // Runs the passed |policy_method| policy and expects it to return the
  // |expected| return value.
  template<typename T, typename R, typename... Args>
  void ExpectPolicyStatus(
      EvalStatus expected,
      T policy_method,
      R* result, Args... args) {
    string error = "<None>";
    eval_ctx_->ResetEvaluation();
    EXPECT_EQ(expected,
              (policy_.*policy_method)(eval_ctx_, &fake_state_, &error, result,
                                       args...))
        << "Returned error: " << error
        << "\nEvaluation context: " << eval_ctx_->DumpContext();
  }

  FakeClock fake_clock_;
  FakeState fake_state_;
  scoped_refptr<EvaluationContext> eval_ctx_;
  ChromeOSPolicy policy_;  // ChromeOSPolicy under test.
};

TEST_F(UmChromeOSPolicyTest, FirstCheckIsAtMostInitialIntervalAfterStart) {
  Time next_update_check;

  // Set the last update time so it'll appear as if this is a first update check
  // in the lifetime of the current updater.
  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(fake_clock_.GetWallclockTime() - TimeDelta::FromMinutes(10)));

  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &ChromeOSPolicy::NextUpdateCheckTime, &next_update_check);

  EXPECT_LE(fake_clock_.GetWallclockTime(), next_update_check);
  EXPECT_GE(
      fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(
          ChromeOSPolicy::kTimeoutInitialInterval +
          ChromeOSPolicy::kTimeoutRegularFuzz / 2),
      next_update_check);
}

TEST_F(UmChromeOSPolicyTest, RecurringCheckBaseIntervalAndFuzz) {
  // Ensure that we're using the correct interval (kPeriodicInterval) and fuzz
  // (kTimeoutRegularFuzz) as base values for period updates.
  Time next_update_check;

  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &ChromeOSPolicy::NextUpdateCheckTime, &next_update_check);

  EXPECT_LE(
      fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(
          ChromeOSPolicy::kTimeoutPeriodicInterval -
          ChromeOSPolicy::kTimeoutRegularFuzz / 2),
      next_update_check);
  EXPECT_GE(
      fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(
          ChromeOSPolicy::kTimeoutPeriodicInterval +
          ChromeOSPolicy::kTimeoutRegularFuzz / 2),
      next_update_check);
}

TEST_F(UmChromeOSPolicyTest, RecurringCheckBackoffIntervalAndFuzz) {
  // Ensure that we're properly backing off and fuzzing in the presence of
  // failed updates attempts.
  Time next_update_check;

  fake_state_.updater_provider()->var_consecutive_failed_update_checks()->
      reset(new unsigned int(2));  // NOLINT(readability/casting)

  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &ChromeOSPolicy::NextUpdateCheckTime, &next_update_check);

  int expected_interval = ChromeOSPolicy::kTimeoutPeriodicInterval * 4;
  EXPECT_LE(
      fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(
          expected_interval - expected_interval / 2),
      next_update_check);
  EXPECT_GE(
      fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(
          expected_interval + expected_interval / 2),
      next_update_check);
}

TEST_F(UmChromeOSPolicyTest, ExponentialBackoffIsCapped) {
  Time next_update_check;

  fake_state_.updater_provider()->var_consecutive_failed_update_checks()->
      reset(new unsigned int(100));  // NOLINT(readability/casting)

  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &ChromeOSPolicy::NextUpdateCheckTime, &next_update_check);

  EXPECT_LE(
      fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(
          ChromeOSPolicy::kTimeoutMaxBackoffInterval -
          ChromeOSPolicy::kTimeoutMaxBackoffInterval / 2),
      next_update_check);
  EXPECT_GE(
      fake_clock_.GetWallclockTime() + TimeDelta::FromSeconds(
          ChromeOSPolicy::kTimeoutMaxBackoffInterval +
          ChromeOSPolicy::kTimeoutMaxBackoffInterval /2),
      next_update_check);
}

TEST_F(UmChromeOSPolicyTest, UpdateCheckAllowedWaitsForTheTimeout) {
  // We get the next update_check timestamp from the policy's private method
  // and then we check the public method respects that value on the normal
  // case.
  Time next_update_check;
  Time last_checked_time =
      fake_clock_.GetWallclockTime() + TimeDelta::FromMinutes(1234);

  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &ChromeOSPolicy::NextUpdateCheckTime, &next_update_check);

  UpdateCheckParams result;

  // Check that the policy blocks until the next_update_check is reached.
  SetUpDefaultClock();
  SetUpDefaultState();
  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  fake_clock_.SetWallclockTime(next_update_check - TimeDelta::FromSeconds(1));
  ExpectPolicyStatus(EvalStatus::kAskMeAgainLater,
                     &Policy::UpdateCheckAllowed, &result);

  SetUpDefaultClock();
  SetUpDefaultState();
  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  fake_clock_.SetWallclockTime(next_update_check + TimeDelta::FromSeconds(1));
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateCheckAllowed, &result);
}

TEST_F(UmChromeOSPolicyTest, UpdateCheckAllowedWithAttributes) {
  // Update check is allowed, reponse includes attributes for use in the
  // request.
  SetUpdateCheckAllowed(true);

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_release_channel_delegated()->
      reset(new bool(false));
  fake_state_.device_policy_provider()->var_release_channel()->
      reset(new string("foo-channel"));

  UpdateCheckParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateCheckAllowed, &result);
  EXPECT_TRUE(result.updates_enabled);
  EXPECT_EQ("foo-channel", result.target_channel);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCheckAllowedUpdatesDisabledForUnofficialBuilds) {
  // UpdateCheckAllowed should return false (kSucceeded) if this is an
  // unofficial build; we don't want periodic update checks on developer images.

  fake_state_.system_provider()->var_is_official_build()->reset(
      new bool(false));

  UpdateCheckParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateCheckAllowed, &result);
  EXPECT_FALSE(result.updates_enabled);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCheckAllowedUpdatesDisabledForRemovableBootDevice) {
  // UpdateCheckAllowed should return false (kSucceeded) if the image booted
  // from a removable device.

  fake_state_.system_provider()->var_is_boot_device_removable()->reset(
      new bool(true));

  UpdateCheckParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateCheckAllowed, &result);
  EXPECT_FALSE(result.updates_enabled);
}

TEST_F(UmChromeOSPolicyTest, UpdateCheckAllowedUpdatesDisabledByPolicy) {
  // UpdateCheckAllowed should return kAskMeAgainLater because a device policy
  // is loaded and prohibits updates.

  SetUpdateCheckAllowed(false);
  fake_state_.device_policy_provider()->var_update_disabled()->reset(
      new bool(true));

  UpdateCheckParams result;
  ExpectPolicyStatus(EvalStatus::kAskMeAgainLater,
                     &Policy::UpdateCheckAllowed, &result);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartFailsCheckAllowedError) {
  // The UpdateCanStart policy fails, not being able to query
  // UpdateCheckAllowed.

  // Configure the UpdateCheckAllowed policy to fail.
  fake_state_.updater_provider()->var_updater_started_time()->reset(nullptr);

  // Check that the UpdateCanStart fails.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kFailed,
                     &Policy::UpdateCanStart, &result, false, update_state);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartNotAllowedCheckDue) {
  // The UpdateCanStart policy returns false because we are due for another
  // update check.

  SetUpdateCheckAllowed(true);

  // Check that the UpdateCanStart returns false.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateCanStart, &result, false, update_state);
  EXPECT_FALSE(result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kCheckDue, result.cannot_start_reason);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartAllowedNoDevicePolicy) {
  // The UpdateCanStart policy returns true; no device policy is loaded.

  SetUpdateCheckAllowed(false);
  fake_state_.device_policy_provider()->var_device_policy_is_loaded()->reset(
      new bool(false));

  // Check that the UpdateCanStart returns true with no further attributes.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateCanStart, &result, false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_FALSE(result.p2p_allowed);
  EXPECT_EQ(0, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartAllowedBlankPolicy) {
  // The UpdateCanStart policy returns true; device policy is loaded but imposes
  // no restrictions on updating.

  SetUpdateCheckAllowed(false);

  // Check that the UpdateCanStart returns true.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateCanStart, &result, false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_FALSE(result.p2p_allowed);
  EXPECT_EQ(0, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartFailsScatteringFailed) {
  // The UpdateCanStart policy fails because the UpdateScattering policy it
  // depends on fails (unset variable).

  SetUpdateCheckAllowed(false);

  // Override the default seed variable with a null value so that the policy
  // request would fail.
  fake_state_.random_provider()->var_seed()->reset(nullptr);

  // Check that the UpdateCanStart fails.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kFailed,
                     &Policy::UpdateCanStart, &result, false, update_state);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCanStartNotAllowedScatteringNewWaitPeriodApplies) {
  // The UpdateCanStart policy returns false; device policy is loaded and
  // scattering applies due to an unsatisfied wait period, which was newly
  // generated.

  SetUpdateCheckAllowed(false);
  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromMinutes(2)));


  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));

  // Check that the UpdateCanStart returns false and a new wait period
  // generated.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_FALSE(result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kScattering, result.cannot_start_reason);
  EXPECT_LT(TimeDelta(), result.scatter_wait_period);
  EXPECT_EQ(0, result.scatter_check_threshold);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCanStartNotAllowedScatteringPrevWaitPeriodStillApplies) {
  // The UpdateCanStart policy returns false w/ kAskMeAgainLater; device policy
  // is loaded and a previously generated scattering period still applies, none
  // of the scattering values has changed.

  SetUpdateCheckAllowed(false);
  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromMinutes(2)));

  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  update_state.scatter_wait_period = TimeDelta::FromSeconds(35);

  // Check that the UpdateCanStart returns false and a new wait period
  // generated.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kAskMeAgainLater, &Policy::UpdateCanStart,
                     &result, false, update_state);
  EXPECT_FALSE(result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kScattering, result.cannot_start_reason);
  EXPECT_EQ(TimeDelta::FromSeconds(35), result.scatter_wait_period);
  EXPECT_EQ(0, result.scatter_check_threshold);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCanStartNotAllowedScatteringNewCountThresholdApplies) {
  // The UpdateCanStart policy returns false; device policy is loaded and
  // scattering applies due to an unsatisfied update check count threshold.
  //
  // This ensures a non-zero check threshold, which may or may not be combined
  // with a non-zero wait period (for which we cannot reliably control).

  SetUpdateCheckAllowed(false);
  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromSeconds(1)));

  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  update_state.scatter_check_threshold_min = 2;
  update_state.scatter_check_threshold_max = 5;

  // Check that the UpdateCanStart returns false.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_FALSE(result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kScattering, result.cannot_start_reason);
  EXPECT_LE(2, result.scatter_check_threshold);
  EXPECT_GE(5, result.scatter_check_threshold);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCanStartNotAllowedScatteringPrevCountThresholdStillApplies) {
  // The UpdateCanStart policy returns false; device policy is loaded and
  // scattering due to a previously generated count threshold still applies.

  SetUpdateCheckAllowed(false);
  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromSeconds(1)));

  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  update_state.scatter_check_threshold = 3;
  update_state.scatter_check_threshold_min = 2;
  update_state.scatter_check_threshold_max = 5;

  // Check that the UpdateCanStart returns false.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_FALSE(result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kScattering, result.cannot_start_reason);
  EXPECT_EQ(3, result.scatter_check_threshold);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartAllowedScatteringSatisfied) {
  // The UpdateCanStart policy returns true; device policy is loaded and
  // scattering is enabled, but both wait period and check threshold are
  // satisfied.

  SetUpdateCheckAllowed(false);
  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromSeconds(120)));

  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(75));
  update_state.num_checks = 4;
  update_state.scatter_wait_period = TimeDelta::FromSeconds(60);
  update_state.scatter_check_threshold = 3;
  update_state.scatter_check_threshold_min = 2;
  update_state.scatter_check_threshold_max = 5;

  // Check that the UpdateCanStart returns true.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_EQ(TimeDelta(), result.scatter_wait_period);
  EXPECT_EQ(0, result.scatter_check_threshold);
  EXPECT_EQ(0, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCanStartAllowedInteractivePreventsScattering) {
  // The UpdateCanStart policy returns true; device policy is loaded and
  // scattering would have applied, except that the update check is interactive
  // and so it is suppressed.

  SetUpdateCheckAllowed(false);
  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromSeconds(1)));

  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  update_state.scatter_check_threshold = 0;
  update_state.scatter_check_threshold_min = 2;
  update_state.scatter_check_threshold_max = 5;

  // Check that the UpdateCanStart returns true.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     true, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_EQ(TimeDelta(), result.scatter_wait_period);
  EXPECT_EQ(0, result.scatter_check_threshold);
  EXPECT_EQ(0, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCanStartAllowedOobePreventsScattering) {
  // The UpdateCanStart policy returns true; device policy is loaded and
  // scattering would have applied, except that OOBE was not completed and so it
  // is suppressed.

  SetUpdateCheckAllowed(false);
  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromSeconds(1)));
  fake_state_.system_provider()->var_is_oobe_complete()->reset(new bool(false));

  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  update_state.scatter_check_threshold = 0;
  update_state.scatter_check_threshold_min = 2;
  update_state.scatter_check_threshold_max = 5;

  // Check that the UpdateCanStart returns true.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     true, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_EQ(TimeDelta(), result.scatter_wait_period);
  EXPECT_EQ(0, result.scatter_check_threshold);
  EXPECT_EQ(0, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartAllowedWithAttributes) {
  // The UpdateCanStart policy returns true; device policy permits both HTTP and
  // P2P updates, as well as a non-empty target channel string.

  SetUpdateCheckAllowed(false);

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
      new bool(true));

  // Check that the UpdateCanStart returns true.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_TRUE(result.p2p_allowed);
  EXPECT_EQ(0, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartAllowedWithP2PFromUpdater) {
  // The UpdateCanStart policy returns true; device policy forbids both HTTP and
  // P2P updates, but the updater is configured to allow P2P and overrules the
  // setting.

  SetUpdateCheckAllowed(false);

  // Override specific device policy attributes.
  fake_state_.updater_provider()->var_p2p_enabled()->reset(new bool(true));

  // Check that the UpdateCanStart returns true.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_TRUE(result.p2p_allowed);
  EXPECT_EQ(0, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCanStartAllowedWithHttpUrlForUnofficialBuild) {
  // The UpdateCanStart policy returns true; device policy forbids both HTTP and
  // P2P updates, but marking this an unofficial build overrules the HTTP
  // setting.

  SetUpdateCheckAllowed(false);

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(false));
  fake_state_.system_provider()->var_is_official_build()->
      reset(new bool(false));

  // Check that the UpdateCanStart returns true.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_FALSE(result.p2p_allowed);
  EXPECT_EQ(0, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartAllowedWithHttpsUrl) {
  // The UpdateCanStart policy returns true; device policy forbids both HTTP and
  // P2P updates, but an HTTPS URL is provided and selected for download.

  SetUpdateCheckAllowed(false);

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(false));

  // Add an HTTPS URL.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  update_state.download_urls.push_back("https://secure/url/");

  // Check that the UpdateCanStart returns true.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_FALSE(result.p2p_allowed);
  EXPECT_EQ(1, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartAllowedWithSecondUrlMaxExceeded) {
  // The UpdateCanStart policy returns true; the first URL exceeded the maximum
  // allowed number of failures, but a second URL is available.

  SetUpdateCheckAllowed(false);

  // Add a second URL; update with this URL attempted and failed enough times to
  // disqualify the current (first) URL. This tests both the previously
  // accounted failures (download_url_num_failures) as well as those occurring
  // since the last call (download_url_error_codes).
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  update_state.num_checks = 10;
  update_state.download_urls.push_back("http://another/fake/url/");
  update_state.download_url_num_failures = 9;
  update_state.download_url_error_codes.push_back(
      ErrorCode::kDownloadTransferError);
  update_state.download_url_error_codes.push_back(
      ErrorCode::kDownloadWriteError);

  // Check that the UpdateCanStart returns true.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_FALSE(result.p2p_allowed);
  EXPECT_EQ(1, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartAllowedWithSecondUrlHardError) {
  // The UpdateCanStart policy returns true; the first URL fails with a hard
  // error, but a second URL is available.

  SetUpdateCheckAllowed(false);

  // Add a second URL; update with this URL attempted and failed in a way that
  // causes it to switch directly to the next URL.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  update_state.num_checks = 10;
  update_state.download_urls.push_back("http://another/fake/url/");
  update_state.download_url_error_codes.push_back(
      ErrorCode::kPayloadHashMismatchError);

  // Check that the UpdateCanStart returns true.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_FALSE(result.p2p_allowed);
  EXPECT_EQ(1, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartAllowedUrlWrapsAround) {
  // The UpdateCanStart policy returns true; URL search properly wraps around
  // the last one on the list.

  SetUpdateCheckAllowed(false);

  // Add a second URL; update with this URL attempted and failed in a way that
  // causes it to switch directly to the next URL.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  update_state.num_checks = 10;
  update_state.download_urls.push_back("http://another/fake/url/");
  update_state.download_url_idx = 1;
  update_state.download_url_error_codes.push_back(
      ErrorCode::kPayloadHashMismatchError);

  // Check that the UpdateCanStart returns true.
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_FALSE(result.p2p_allowed);
  EXPECT_EQ(0, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartNotAllowedNoUsableUrls) {
  // The UpdateCanStart policy returns false; there's a single HTTP URL but its
  // use is forbidden by policy.

  SetUpdateCheckAllowed(false);

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(false));

  // Check that the UpdateCanStart returns false.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kFailed, &Policy::UpdateCanStart, &result,
                     false, update_state);
}

TEST_F(UmChromeOSPolicyTest, UpdateCanStartAllowedNoUsableUrlsButP2PEnabled) {
  // The UpdateCanStart policy returns true; there's a single HTTP URL but its
  // use is forbidden by policy, however P2P is enabled. The result indicates
  // that no URL can be used.

  SetUpdateCheckAllowed(false);

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(false));

  // Check that the UpdateCanStart returns true.
  UpdateState update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  UpdateDownloadParams result;
  ExpectPolicyStatus(EvalStatus::kSucceeded, &Policy::UpdateCanStart, &result,
                     false, update_state);
  EXPECT_TRUE(result.update_can_start);
  EXPECT_TRUE(result.p2p_allowed);
  EXPECT_GT(0, result.download_url_idx);
  EXPECT_EQ(0, result.download_url_num_failures);
}

TEST_F(UmChromeOSPolicyTest, UpdateDownloadAllowedEthernetDefault) {
  // Ethernet is always allowed.

  fake_state_.shill_provider()->var_conn_type()->
      reset(new ConnectionType(ConnectionType::kEthernet));

  bool result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateDownloadAllowed, &result);
  EXPECT_TRUE(result);
}

TEST_F(UmChromeOSPolicyTest, UpdateDownloadAllowedWifiDefault) {
  // Wifi is allowed if not tethered.

  fake_state_.shill_provider()->var_conn_type()->
      reset(new ConnectionType(ConnectionType::kWifi));

  bool result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateDownloadAllowed, &result);
  EXPECT_TRUE(result);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCurrentConnectionNotAllowedWifiTetheredDefault) {
  // Tethered wifi is not allowed by default.

  fake_state_.shill_provider()->var_conn_type()->
      reset(new ConnectionType(ConnectionType::kWifi));
  fake_state_.shill_provider()->var_conn_tethering()->
      reset(new ConnectionTethering(ConnectionTethering::kConfirmed));

  bool result;
  ExpectPolicyStatus(EvalStatus::kAskMeAgainLater,
                     &Policy::UpdateDownloadAllowed, &result);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateDownloadAllowedWifiTetheredPolicyOverride) {
  // Tethered wifi can be allowed by policy.

  fake_state_.shill_provider()->var_conn_type()->
      reset(new ConnectionType(ConnectionType::kWifi));
  fake_state_.shill_provider()->var_conn_tethering()->
      reset(new ConnectionTethering(ConnectionTethering::kConfirmed));
  set<ConnectionType> allowed_connections;
  allowed_connections.insert(ConnectionType::kCellular);
  fake_state_.device_policy_provider()->
      var_allowed_connection_types_for_update()->
      reset(new set<ConnectionType>(allowed_connections));

  bool result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateDownloadAllowed, &result);
  EXPECT_TRUE(result);
}

TEST_F(UmChromeOSPolicyTest, UpdateDownloadAllowedWimaxDefault) {
  // Wimax is always allowed.

  fake_state_.shill_provider()->var_conn_type()->
      reset(new ConnectionType(ConnectionType::kWifi));

  bool result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateDownloadAllowed, &result);
  EXPECT_TRUE(result);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCurrentConnectionNotAllowedBluetoothDefault) {
  // Bluetooth is never allowed.

  fake_state_.shill_provider()->var_conn_type()->
      reset(new ConnectionType(ConnectionType::kBluetooth));

  bool result;
  ExpectPolicyStatus(EvalStatus::kAskMeAgainLater,
                     &Policy::UpdateDownloadAllowed, &result);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCurrentConnectionNotAllowedBluetoothPolicyCannotOverride) {
  // Bluetooth cannot be allowed even by policy.

  fake_state_.shill_provider()->var_conn_type()->
      reset(new ConnectionType(ConnectionType::kBluetooth));
  set<ConnectionType> allowed_connections;
  allowed_connections.insert(ConnectionType::kBluetooth);
  fake_state_.device_policy_provider()->
      var_allowed_connection_types_for_update()->
      reset(new set<ConnectionType>(allowed_connections));

  bool result;
  ExpectPolicyStatus(EvalStatus::kAskMeAgainLater,
                     &Policy::UpdateDownloadAllowed, &result);
}

TEST_F(UmChromeOSPolicyTest, UpdateCurrentConnectionNotAllowedCellularDefault) {
  // Cellular is not allowed by default.

  fake_state_.shill_provider()->var_conn_type()->
      reset(new ConnectionType(ConnectionType::kCellular));

  bool result;
  ExpectPolicyStatus(EvalStatus::kAskMeAgainLater,
                     &Policy::UpdateDownloadAllowed, &result);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateDownloadAllowedCellularPolicyOverride) {
  // Update over cellular can be enabled by policy.

  fake_state_.shill_provider()->var_conn_type()->
      reset(new ConnectionType(ConnectionType::kCellular));
  set<ConnectionType> allowed_connections;
  allowed_connections.insert(ConnectionType::kCellular);
  fake_state_.device_policy_provider()->
      var_allowed_connection_types_for_update()->
      reset(new set<ConnectionType>(allowed_connections));

  bool result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateDownloadAllowed, &result);
  EXPECT_TRUE(result);
}

TEST_F(UmChromeOSPolicyTest,
       UpdateDownloadAllowedCellularUserOverride) {
  // Update over cellular can be enabled by user settings, but only if policy
  // is present and does not determine allowed connections.

  fake_state_.shill_provider()->var_conn_type()->
      reset(new ConnectionType(ConnectionType::kCellular));
  set<ConnectionType> allowed_connections;
  allowed_connections.insert(ConnectionType::kCellular);
  fake_state_.updater_provider()->var_cellular_enabled()->
      reset(new bool(true));

  bool result;
  ExpectPolicyStatus(EvalStatus::kSucceeded,
                     &Policy::UpdateDownloadAllowed, &result);
  EXPECT_TRUE(result);
}

}  // namespace chromeos_update_manager
