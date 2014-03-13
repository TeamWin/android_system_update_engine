// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/time.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/policy_manager/real_time_provider.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::FakeClock;

namespace chromeos_policy_manager {

class PmRealTimeProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // The provider initializes correctly.
    provider_.reset(new RealTimeProvider(&fake_clock_));
    PMTEST_ASSERT_NOT_NULL(provider_.get());
    ASSERT_TRUE(provider_->Init());
  }

  // Generates a fixed timestamp for use in faking the current time.
  Time CurrTime() {
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

  const TimeDelta default_timeout_ = TimeDelta::FromSeconds(1);
  FakeClock fake_clock_;
  scoped_ptr<RealTimeProvider> provider_;
};

TEST_F(PmRealTimeProviderTest, CurrDateValid) {
  const Time now = CurrTime();
  Time::Exploded expected;
  now.LocalExplode(&expected);
  fake_clock_.SetWallclockTime(now);
  scoped_ptr<const Time> curr_date(
      provider_->var_curr_date()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(curr_date.get());

  Time::Exploded actual;
  curr_date->LocalExplode(&actual);
  EXPECT_EQ(expected.year, actual.year);
  EXPECT_EQ(expected.month, actual.month);
  EXPECT_EQ(expected.day_of_week, actual.day_of_week);
  EXPECT_EQ(expected.day_of_month, actual.day_of_month);
  EXPECT_EQ(0, actual.hour);
  EXPECT_EQ(0, actual.minute);
  EXPECT_EQ(0, actual.second);
  EXPECT_EQ(0, actual.millisecond);
}

TEST_F(PmRealTimeProviderTest, CurrHourValid) {
  const Time now = CurrTime();
  Time::Exploded expected;
  now.LocalExplode(&expected);
  fake_clock_.SetWallclockTime(now);
  scoped_ptr<const int> curr_hour(
      provider_->var_curr_hour()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(curr_hour.get());

  EXPECT_EQ(expected.hour, *curr_hour);
}

}  // namespace chromeos_policy_manager
