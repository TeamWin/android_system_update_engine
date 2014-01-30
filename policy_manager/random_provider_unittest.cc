// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "policy_manager/random_provider.h"
#include "policy_manager/random_vars.h"
#include "policy_manager/pmtest_utils.h"

using base::TimeDelta;
using std::string;

namespace chromeos_policy_manager {

class PmRandomProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    default_timeout_ = TimeDelta::FromSeconds(1);

    // All variables are initially null.
    PMTEST_ASSERT_NULL(var_random_seed);

    // The provider initializes correctly.
    provider_ = new RandomProvider();
    PMTEST_ASSERT_NOT_NULL(provider_);
    ASSERT_TRUE(provider_->Init());

    // The provider initializes all variables with valid objects.
    PMTEST_EXPECT_NOT_NULL(var_random_seed);
  }

  virtual void TearDown() {
    delete provider_;
    provider_ = NULL;
    PMTEST_EXPECT_NULL(var_random_seed);
  }

  TimeDelta default_timeout_;

 private:
  RandomProvider* provider_;
};


TEST_F(PmRandomProviderTest, GetRandomValues) {
  // Should not return the same random seed repeatedly.
  scoped_ptr<const uint64_t> value(
      var_random_seed->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(value.get());

  // Test that at least the returned values are different. This test fails,
  // by design, once every 2^320 runs.
  bool is_same_value = true;
  for (int i = 0; i < 5; i++) {
    scoped_ptr<const uint64_t> other_value(
        var_random_seed->GetValue(default_timeout_, NULL));
    PMTEST_ASSERT_NOT_NULL(other_value.get());
    is_same_value = is_same_value && *other_value == *value;
  }
  EXPECT_FALSE(is_same_value);
}

}  // namespace chromeos_policy_manager
