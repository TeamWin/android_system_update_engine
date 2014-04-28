// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/prng.h"

#include <vector>

#include <gtest/gtest.h>

using std::vector;

namespace chromeos_policy_manager {

TEST(PmPRNGTest, ShouldBeDeterministic) {
  PRNG a(42);
  PRNG b(42);

  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(a.rand(), b.rand()) << "Iteration i=" << i;
  }
}

TEST(PmPRNGTest, SeedChangesGeneratedSequence) {
  PRNG a(42);
  PRNG b(5);

  vector<int> values_a;
  vector<int> values_b;

  for (int i = 0; i < 100; ++i) {
    values_a.push_back(a.rand());
    values_b.push_back(b.rand());
  }
  EXPECT_NE(values_a, values_b);
}

TEST(PmPRNGTest, IsNotConstant) {
  PRNG prng(5);

  int initial_value = prng.rand();
  bool prng_is_constant = true;
  for (int i = 0; i < 100; ++i) {
    if (prng.rand() != initial_value) {
      prng_is_constant = false;
      break;
    }
  }
  EXPECT_FALSE(prng_is_constant) << "After 100 iterations.";
}

}  // namespace chromeos_policy_manager
