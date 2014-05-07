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
    EXPECT_EQ(a.Rand(), b.Rand()) << "Iteration i=" << i;
  }
}

TEST(PmPRNGTest, SeedChangesGeneratedSequence) {
  PRNG a(42);
  PRNG b(5);

  vector<uint32_t> values_a;
  vector<uint32_t> values_b;

  for (int i = 0; i < 100; ++i) {
    values_a.push_back(a.Rand());
    values_b.push_back(b.Rand());
  }
  EXPECT_NE(values_a, values_b);
}

TEST(PmPRNGTest, IsNotConstant) {
  PRNG prng(5);

  uint32_t initial_value = prng.Rand();
  bool prng_is_constant = true;
  for (int i = 0; i < 100; ++i) {
    if (prng.Rand() != initial_value) {
      prng_is_constant = false;
      break;
    }
  }
  EXPECT_FALSE(prng_is_constant) << "After 100 iterations.";
}

TEST(PmPRNGTest, RandCoversRange) {
  PRNG a(42);
  int hits[11] = { 0 };

  for (int i = 0; i < 1000; i++) {
    int r = a.RandMinMax(0, 10);
    ASSERT_LE(0, r);
    ASSERT_GE(10, r);
    hits[r]++;
  }

  for (auto& hit : hits)
    EXPECT_LT(0, hit);
}

}  // namespace chromeos_policy_manager
