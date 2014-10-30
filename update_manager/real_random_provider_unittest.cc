// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <memory>

#include "update_engine/update_manager/real_random_provider.h"
#include "update_engine/update_manager/umtest_utils.h"

using std::unique_ptr;

namespace chromeos_update_manager {

class UmRealRandomProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // The provider initializes correctly.
    provider_.reset(new RealRandomProvider());
    ASSERT_NE(nullptr, provider_.get());
    ASSERT_TRUE(provider_->Init());

    provider_->var_seed();
  }

  unique_ptr<RealRandomProvider> provider_;
};

TEST_F(UmRealRandomProviderTest, InitFinalize) {
  // The provider initializes all variables with valid objects.
  EXPECT_NE(nullptr, provider_->var_seed());
}

TEST_F(UmRealRandomProviderTest, GetRandomValues) {
  // Should not return the same random seed repeatedly.
  unique_ptr<const uint64_t> value(
      provider_->var_seed()->GetValue(UmTestUtils::DefaultTimeout(), nullptr));
  ASSERT_NE(nullptr, value.get());

  // Test that at least the returned values are different. This test fails,
  // by design, once every 2^320 runs.
  bool is_same_value = true;
  for (int i = 0; i < 5; i++) {
    unique_ptr<const uint64_t> other_value(
        provider_->var_seed()->GetValue(UmTestUtils::DefaultTimeout(),
                                        nullptr));
    ASSERT_NE(nullptr, other_value.get());
    is_same_value = is_same_value && *other_value == *value;
  }
  EXPECT_FALSE(is_same_value);
}

}  // namespace chromeos_update_manager
