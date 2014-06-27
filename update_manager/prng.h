// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_PRNG_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_PRNG_H_

#include <random>

#include <base/logging.h>

namespace chromeos_update_manager {

// A thread-safe, unsecure, 32-bit pseudo-random number generator based on
// std::mt19937.
class PRNG {
 public:
  // Initializes the generator with the passed |seed| value.
  explicit PRNG(uint32_t seed) : gen_(seed) {}

  // Returns a random unsigned 32-bit integer.
  uint32_t Rand() { return gen_(); }

  // Returns a random integer uniformly distributed in the range [min, max].
  int RandMinMax(int min, int max) {
    DCHECK_LE(min, max);
    return std::uniform_int_distribution<>(min, max)(gen_);
  }

 private:
  // A pseudo-random number generator.
  std::mt19937 gen_;

  DISALLOW_COPY_AND_ASSIGN(PRNG);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_PRNG_H_
