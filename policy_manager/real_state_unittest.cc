// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_state.h"

#include <gtest/gtest.h>

#include "update_engine/policy_manager/fake_random_provider.h"
#include "update_engine/policy_manager/fake_shill_provider.h"
#include "update_engine/policy_manager/fake_system_provider.h"
#include "update_engine/policy_manager/fake_time_provider.h"
#include "update_engine/policy_manager/pmtest_utils.h"

namespace chromeos_policy_manager {

TEST(PmRealStateTest, InitTest) {
  RealState state(new FakeRandomProvider(),
                  new FakeShillProvider(),
                  new FakeSystemProvider(),
                  new FakeTimeProvider());
  EXPECT_TRUE(state.Init());

  // Check that the providers are being initialized. Beyond ensuring that we get
  // non-null provider handles, verifying that we can get a single variable from
  // each provider is enough of an indication that it has initialized.
  PMTEST_ASSERT_NOT_NULL(state.random_provider());
  PMTEST_EXPECT_NOT_NULL(state.random_provider()->var_seed());
  PMTEST_ASSERT_NOT_NULL(state.shill_provider());
  PMTEST_EXPECT_NOT_NULL(state.shill_provider()->var_is_connected());
  PMTEST_ASSERT_NOT_NULL(state.system_provider());
  PMTEST_ASSERT_NOT_NULL(state.system_provider()->var_is_normal_boot_mode());
  PMTEST_ASSERT_NOT_NULL(state.time_provider());
  PMTEST_ASSERT_NOT_NULL(state.time_provider()->var_curr_date());
}

}  // namespace chromeos_policy_manager
