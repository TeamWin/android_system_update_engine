// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <base/time.h>
#include <gtest/gtest.h>

#include "update_engine/policy_manager/real_shill_provider.h"
#include "update_engine/policy_manager/pmtest_utils.h"

using base::Time;
using base::TimeDelta;

namespace chromeos_policy_manager {

class PmRealShillProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    default_timeout_ = TimeDelta::FromSeconds(1);

    // The provider initializes correctly.
    time_before_init_ = Time::Now();
    provider_.reset(new RealShillProvider());
    time_after_init_ = Time::Now();
    PMTEST_ASSERT_NOT_NULL(provider_.get());
    ASSERT_TRUE(provider_->Init());
  }

  TimeDelta default_timeout_;
  Time time_before_init_;
  Time time_after_init_;
  scoped_ptr<RealShillProvider> provider_;
};


TEST_F(PmRealShillProviderTest, DefaultValues) {
  // Tests that initial values were set correctly.
  scoped_ptr<const bool> is_connected(
      provider_->var_is_connected()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(is_connected.get());
  EXPECT_FALSE(*is_connected);

  scoped_ptr<const ShillConnType> conn_type(
      provider_->var_conn_type()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(conn_type.get());
  EXPECT_EQ(kShillConnTypeUnknown, *conn_type);

  scoped_ptr<const Time> conn_last_changed(
      provider_->var_conn_last_changed()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(conn_last_changed.get());
  EXPECT_LE(time_before_init_, *conn_last_changed);
  EXPECT_GE(time_after_init_, *conn_last_changed);
}

}  // namespace chromeos_policy_manager
