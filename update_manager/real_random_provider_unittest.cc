// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "update_engine/update_manager/real_random_provider.h"
#include "update_engine/update_manager/umtest_utils.h"

using base::TimeDelta;

namespace chromeos_update_manager {

class UmRealRandomProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // The provider initializes correctly.
    provider_.reset(new RealRandomProvider());
    UMTEST_ASSERT_NOT_NULL(provider_.get());
    ASSERT_TRUE(provider_->Init());

    provider_->var_seed();
  }

  scoped_ptr<RealRandomProvider> provider_;
};

TEST_F(UmRealRandomProviderTest, InitFinalize) {
  // The provider initializes all variables with valid objects.
  UMTEST_EXPECT_NOT_NULL(provider_->var_seed());
}

TEST_F(UmRealRandomProviderTest, GetRandomValues) {
  // Should not return the same random seed repeatedly.
  scoped_ptr<const uint64_t> value(
      provider_->var_seed()->GetValue(UmTestUtils::DefaultTimeout(), nullptr));
  UMTEST_ASSERT_NOT_NULL(value.get());

  // Test that at least the returned values are different. This test fails,
  // by design, once every 2^320 runs.
  bool is_same_value = true;
  for (int i = 0; i < 5; i++) {
    scoped_ptr<const uint64_t> other_value(
        provider_->var_seed()->GetValue(UmTestUtils::DefaultTimeout(),
                                        nullptr));
    UMTEST_ASSERT_NOT_NULL(other_value.get());
    is_same_value = is_same_value && *other_value == *value;
  }
  EXPECT_FALSE(is_same_value);
}

}  // namespace chromeos_update_manager
