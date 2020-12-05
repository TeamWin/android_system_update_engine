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

#include <memory>

#include <base/optional.h>
#include <base/time/time.h>
#include <base/test/simple_test_clock.h>
#include <brillo/message_loops/fake_message_loop.h>
#include <brillo/message_loops/message_loop_utils.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "update_engine/cros/fake_system_state.h"
#include "update_engine/update_manager/fake_state.h"
#include "update_engine/update_manager/update_time_restrictions_monitor.h"

using brillo::FakeMessageLoop;
using brillo::MessageLoop;
using brillo::MessageLoopRunMaxIterations;
using chromeos_update_engine::FakeSystemState;

namespace chromeos_update_manager {

namespace {

constexpr base::TimeDelta kDurationOffset = base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kHourDuration = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kMinuteDuration = base::TimeDelta::FromMinutes(1);
// Initial time: Monday, May 4th 2020 8:13 AM before interval.
constexpr base::Time::Exploded kInitialTimeBeforeInterval{
    2020, 5, 0, 4, 10, 13, 0, 0};
// Initial time: Monday, May 4th 2020 10:20 AM within interval.
constexpr base::Time::Exploded kInitialTimeWithinInterval{
    2020, 5, 0, 4, 10, 20, 0, 0};
const int current_restricted_interval_index = 0;

const WeeklyTimeIntervalVector kTestOneDisallowedTimeIntervals{
    // Monday 8:15 AM to Monday 9:30 PM.
    WeeklyTimeInterval(WeeklyTime(1, kHourDuration * 8 + kMinuteDuration * 15),
                       WeeklyTime(1, kHourDuration * 9 + kMinuteDuration * 30)),
};

const WeeklyTimeIntervalVector kTestTwoDisallowedTimeIntervals{
    // Monday 10:15 AM to Monday 3:30 PM.
    WeeklyTimeInterval(
        WeeklyTime(1, kHourDuration * 10 + kMinuteDuration * 15),
        WeeklyTime(1, kHourDuration * 15 + kMinuteDuration * 30)),
    // Wednesday 8:30 PM to Thursday 8:40 AM.
    WeeklyTimeInterval(WeeklyTime(3, kHourDuration * 20 + kMinuteDuration * 30),
                       WeeklyTime(4, kHourDuration * 8 + kMinuteDuration * 40)),
};

}  // namespace

class MockUpdateTimeRestrictionsMonitorDelegate
    : public UpdateTimeRestrictionsMonitor::Delegate {
 public:
  virtual ~MockUpdateTimeRestrictionsMonitorDelegate() = default;

  MOCK_METHOD0(OnRestrictedIntervalStarts, void());
};

class UmUpdateTimeRestrictionsMonitorTest : public ::testing::Test {
 protected:
  UmUpdateTimeRestrictionsMonitorTest() {
    fake_loop_.SetAsCurrent();
    FakeSystemState::CreateInstance();
  }

  void TearDown() override { EXPECT_FALSE(fake_loop_.PendingTasks()); }

  bool SetNow(const base::Time::Exploded& exploded_now) {
    base::Time now;
    if (!base::Time::FromLocalExploded(exploded_now, &now))
      return false;

    test_clock_.SetNow(now);
    FakeSystemState::Get()->fake_clock()->SetWallclockTime(now);
    return true;
  }

  void AdvanceAfterTimestamp(const WeeklyTime& timestamp) {
    const WeeklyTime now = WeeklyTime::FromTime(test_clock_.Now());
    const base::TimeDelta duration =
        now.GetDurationTo(timestamp) + kDurationOffset;
    test_clock_.Advance(duration);
    FakeSystemState::Get()->fake_clock()->SetWallclockTime(test_clock_.Now());
  }

  void VerifyExpectationsOnDelegate() {
    testing::Mock::VerifyAndClearExpectations(&mock_delegate_);
  }

  void UpdateRestrictedIntervals(const WeeklyTimeIntervalVector& policy_value) {
    auto* policy_variable =
        fake_state_.device_policy_provider()->var_disallowed_time_intervals();
    policy_variable->reset(new WeeklyTimeIntervalVector(policy_value));
    policy_variable->NotifyValueChanged();
  }

  bool IsMonitoringInterval() {
    return monitor_.has_value() && monitor_.value().IsMonitoringInterval();
  }

  void BuildMonitorAndVerify(const WeeklyTimeIntervalVector* policy_value,
                             bool expect_delegate_called,
                             bool expect_monitoring) {
    if (expect_delegate_called)
      EXPECT_CALL(mock_delegate_, OnRestrictedIntervalStarts()).Times(1);
    else
      EXPECT_CALL(mock_delegate_, OnRestrictedIntervalStarts()).Times(0);

    fake_state_.device_policy_provider()
        ->var_disallowed_time_intervals()
        ->reset(policy_value != nullptr
                    ? new WeeklyTimeIntervalVector(*policy_value)
                    : nullptr);
    monitor_.emplace(fake_state_.device_policy_provider(), &mock_delegate_);
    if (expect_delegate_called)
      MessageLoopRunMaxIterations(MessageLoop::current(), 10);
    VerifyExpectationsOnDelegate();

    if (expect_monitoring)
      EXPECT_TRUE(IsMonitoringInterval());
    else
      EXPECT_FALSE(IsMonitoringInterval());
  }

  base::SimpleTestClock test_clock_;
  FakeMessageLoop fake_loop_{&test_clock_};
  FakeState fake_state_;
  MockUpdateTimeRestrictionsMonitorDelegate mock_delegate_;
  base::Optional<UpdateTimeRestrictionsMonitor> monitor_;
};

TEST_F(UmUpdateTimeRestrictionsMonitorTest, PolicyIsNotSet) {
  BuildMonitorAndVerify(
      nullptr, /*expect_delegate_called=*/false, /*expect_monitoring=*/false);
}

TEST_F(UmUpdateTimeRestrictionsMonitorTest, PolicyHasEmptyIntervalList) {
  WeeklyTimeIntervalVector empty_policy;
  BuildMonitorAndVerify(&empty_policy,
                        /*expect_delegate_called=*/false,
                        /*expect_monitoring=*/false);
}

TEST_F(UmUpdateTimeRestrictionsMonitorTest,
       CurrentTimeOutsideOfRestrictedInterval) {
  ASSERT_TRUE(SetNow(kInitialTimeBeforeInterval));
  BuildMonitorAndVerify(&kTestTwoDisallowedTimeIntervals,
                        /*expect_delegate_called=*/false,
                        /*expect_monitoring=*/true);

  // Monitor should only notify start when passing start of interval.
  EXPECT_CALL(mock_delegate_, OnRestrictedIntervalStarts()).Times(1);
  AdvanceAfterTimestamp(
      kTestTwoDisallowedTimeIntervals[current_restricted_interval_index]
          .start());
  MessageLoopRunMaxIterations(MessageLoop::current(), 10);
  VerifyExpectationsOnDelegate();
}

TEST_F(UmUpdateTimeRestrictionsMonitorTest,
       CurrentTimeWithinRestrictedInterval) {
  // Monitor should notify start when it is built with current
  // time within interval.
  ASSERT_TRUE(SetNow(kInitialTimeWithinInterval));
  BuildMonitorAndVerify(&kTestTwoDisallowedTimeIntervals,
                        /*expect_delegate_called=*/true,
                        /*expect_monitoring=*/false);
}

TEST_F(UmUpdateTimeRestrictionsMonitorTest,
       PolicyChangeFromNotSetToOutsideInterval) {
  // Build monitor with empty initial list of intervals.
  BuildMonitorAndVerify(
      nullptr, /*expect_delegate_called=*/false, /*expect_monitoring=*/false);

  // Monitor should not do any notification right after intervals update.
  ASSERT_TRUE(SetNow(kInitialTimeBeforeInterval));
  EXPECT_CALL(mock_delegate_, OnRestrictedIntervalStarts()).Times(0);
  UpdateRestrictedIntervals(kTestTwoDisallowedTimeIntervals);
  MessageLoopRunMaxIterations(MessageLoop::current(), 10);
  VerifyExpectationsOnDelegate();
  EXPECT_TRUE(IsMonitoringInterval());

  // Advance time within new interval and check that notification happen.
  EXPECT_CALL(mock_delegate_, OnRestrictedIntervalStarts()).Times(1);
  AdvanceAfterTimestamp(
      kTestTwoDisallowedTimeIntervals[current_restricted_interval_index]
          .start());
  MessageLoopRunMaxIterations(MessageLoop::current(), 10);
  VerifyExpectationsOnDelegate();
}

TEST_F(UmUpdateTimeRestrictionsMonitorTest,
       PolicyChangeFromNotSetToWithinInterval) {
  // Build monitor with empty initial list of intervals.
  BuildMonitorAndVerify(
      nullptr, /*expect_delegate_called=*/false, /*expect_monitoring=*/false);

  // Advance time inside upcoming new interval and update the intervals.
  // Monitor should immediately notify about started interval.
  ASSERT_TRUE(SetNow(kInitialTimeWithinInterval));
  EXPECT_CALL(mock_delegate_, OnRestrictedIntervalStarts()).Times(1);
  UpdateRestrictedIntervals(kTestTwoDisallowedTimeIntervals);
  MessageLoopRunMaxIterations(MessageLoop::current(), 10);
  VerifyExpectationsOnDelegate();
}

TEST_F(UmUpdateTimeRestrictionsMonitorTest,
       PolicyChangeFromNotSetToEmptyInterval) {
  BuildMonitorAndVerify(
      nullptr, /*expect_delegate_called=*/false, /*expect_monitoring=*/false);

  EXPECT_CALL(mock_delegate_, OnRestrictedIntervalStarts()).Times(0);
  UpdateRestrictedIntervals(WeeklyTimeIntervalVector());
  MessageLoopRunMaxIterations(MessageLoop::current(), 10);
  VerifyExpectationsOnDelegate();
  EXPECT_FALSE(IsMonitoringInterval());
}

TEST_F(UmUpdateTimeRestrictionsMonitorTest,
       PolicyChangeFromOneOutsideIntervalToAnother) {
  // Build monitor with current time outside the intervals.
  BuildMonitorAndVerify(&kTestTwoDisallowedTimeIntervals,
                        /*expect_delegate_called=*/false,
                        /*expect_monitoring=*/true);

  // Update the intervals to outide of current time and no notification should
  // happen yet.
  EXPECT_CALL(mock_delegate_, OnRestrictedIntervalStarts()).Times(0);
  UpdateRestrictedIntervals(kTestOneDisallowedTimeIntervals);
  MessageLoopRunMaxIterations(MessageLoop::current(), 10);
  VerifyExpectationsOnDelegate();

  // Advance time within new interval. Monitor should notify about started
  // interval.
  EXPECT_CALL(mock_delegate_, OnRestrictedIntervalStarts()).Times(1);
  AdvanceAfterTimestamp(
      kTestOneDisallowedTimeIntervals[current_restricted_interval_index]
          .start());
  MessageLoopRunMaxIterations(MessageLoop::current(), 10);
  VerifyExpectationsOnDelegate();
}

TEST_F(UmUpdateTimeRestrictionsMonitorTest,
       PolicyChangeFromOutsideIntervalToWithin) {
  ASSERT_TRUE(SetNow(kInitialTimeWithinInterval));

  // Build monitor with current time outside the intervals.
  BuildMonitorAndVerify(&kTestOneDisallowedTimeIntervals,
                        /*expect_delegate_called=*/false,
                        /*expect_monitoring=*/true);

  // Update interval such that current time is within it. Monitor should notify
  // about started interval.
  EXPECT_CALL(mock_delegate_, OnRestrictedIntervalStarts()).Times(1);
  UpdateRestrictedIntervals(kTestTwoDisallowedTimeIntervals);
  MessageLoopRunMaxIterations(MessageLoop::current(), 10);
  VerifyExpectationsOnDelegate();
}

}  // namespace chromeos_update_manager
