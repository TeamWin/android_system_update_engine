// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/policy_manager/real_random_provider.h"

using base::TimeDelta;

namespace chromeos_policy_manager {

class PmRealRandomProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // The provider initializes correctly.
    provider_.reset(new RealRandomProvider());
    PMTEST_ASSERT_NOT_NULL(provider_.get());
    ASSERT_TRUE(provider_->Init());

    provider_->var_seed();
  }

  scoped_ptr<RealRandomProvider> provider_;
};

TEST_F(PmRealRandomProviderTest, InitFinalize) {
  // The provider initializes all variables with valid objects.
  PMTEST_EXPECT_NOT_NULL(provider_->var_seed());
}

TEST_F(PmRealRandomProviderTest, GetRandomValues) {
  // Should not return the same random seed repeatedly.
  scoped_ptr<const uint64_t> value(
      provider_->var_seed()->GetValue(PmTestUtils::DefaultTimeout(), nullptr));
  PMTEST_ASSERT_NOT_NULL(value.get());

  // Test that at least the returned values are different. This test fails,
  // by design, once every 2^320 runs.
  bool is_same_value = true;
  for (int i = 0; i < 5; i++) {
    scoped_ptr<const uint64_t> other_value(
        provider_->var_seed()->GetValue(PmTestUtils::DefaultTimeout(),
                                        nullptr));
    PMTEST_ASSERT_NOT_NULL(other_value.get());
    is_same_value = is_same_value && *other_value == *value;
  }
  EXPECT_FALSE(is_same_value);
}

}  // namespace chromeos_policy_manager
