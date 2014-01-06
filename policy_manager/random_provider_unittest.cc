// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>
#include <string>

#include "policy_manager/random_provider.h"
#include "policy_manager/random_vars.h"

using base::TimeDelta;
using std::string;

namespace chromeos_policy_manager {

class PMRandomProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(var_random_seed == NULL);
    EXPECT_TRUE(provider_.Init());
  }

  virtual void TearDown() {
    provider_.Finalize();
    ASSERT_TRUE(var_random_seed == NULL);
  }

 private:
  RandomProvider provider_;
};

TEST_F(PMRandomProviderTest, InitFinalize) {
  // The provider should initialize the variable with a valid object.
  ASSERT_TRUE(var_random_seed != NULL);
}

TEST_F(PMRandomProviderTest, GetRandomValues) {
  string errmsg;
  scoped_ptr<const uint64> value(
      var_random_seed->GetValue(TimeDelta::FromSeconds(1.), &errmsg));
  ASSERT_TRUE(value != NULL);

  bool returns_allways_the_same_value = true;
  // Test that at least the returned values are different. This test fails,
  // by design, once every 2^320 runs.
  for (int i = 0; i < 5; i++) {
    scoped_ptr<const uint64> other_value(
        var_random_seed->GetValue(TimeDelta::FromSeconds(1.), &errmsg));
    ASSERT_TRUE(other_value != NULL);
    returns_allways_the_same_value = returns_allways_the_same_value &&
        *other_value == *value;
  }
  EXPECT_FALSE(returns_allways_the_same_value);
}

}  // namespace chromeos_policy_manager
