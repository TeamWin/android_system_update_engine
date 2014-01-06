// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PM_RANDOM_PROVIDER_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PM_RANDOM_PROVIDER_H

#include "base/basictypes.h"

namespace chromeos_policy_manager {

// Provider of random values.
class RandomProvider {
 public:
  RandomProvider() {}

  // Initialize the provider variables and internal state. Returns whether it
  // succeeded.
  bool Init();

  // Destroy all the provider variables in a best-effor approach.
  void Finalize();

 private:
  DISALLOW_COPY_AND_ASSIGN(RandomProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PM_RANDOM_PROVIDER_H
