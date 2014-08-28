// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/update_manager/real_time_provider.h"
#include "update_engine/update_manager/umtest_utils.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::FakeClock;

namespace chromeos_update_manager {

class UmRealTimeProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // The provider initializes correctly.
    provider_.reset(new RealTimeProvider(&fake_clock_));
    ASSERT_NE(nullptr, provider_.get());
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

  FakeClock fake_clock_;
  scoped_ptr<RealTimeProvider> provider_;
};

TEST_F(UmRealTimeProviderTest, CurrDateValid) {
  const Time now = CurrTime();
  Time::Exploded exploded;
  now.LocalExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  const Time expected = Time::FromLocalExploded(exploded);

  fake_clock_.SetWallclockTime(now);
  UmTestUtils::ExpectVariableHasValue(expected, provider_->var_curr_date());
}

TEST_F(UmRealTimeProviderTest, CurrHourValid) {
  const Time now = CurrTime();
  Time::Exploded expected;
  now.LocalExplode(&expected);
  fake_clock_.SetWallclockTime(now);
  UmTestUtils::ExpectVariableHasValue(expected.hour,
                                      provider_->var_curr_hour());
}

}  // namespace chromeos_update_manager
