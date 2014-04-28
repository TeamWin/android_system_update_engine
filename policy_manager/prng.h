// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PRNG_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PRNG_H_

#include <cstdlib>

#include <base/basictypes.h>

namespace chromeos_policy_manager {

// An unsecure Pseudo-Random Number Generator class based on rand_r(3), which
// is thread safe and doesn't interfere with other calls to rand().
class PRNG {
 public:
  // Creates the object using the passed |seed| value as the initial state.
  explicit PRNG(unsigned int seed) : state_(seed) {}

  // Returns a pseudo-random integer in the range [0, RAND_MAX].
  int rand() { return rand_r(&state_); }

 private:
  // The internal state of the PRNG.
  unsigned int state_;

  DISALLOW_COPY_AND_ASSIGN(PRNG);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PRNG_H_
