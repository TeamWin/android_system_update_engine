// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_RANDOM_PROVIDER_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_RANDOM_PROVIDER_H

#include "update_engine/policy_manager/fake_variable.h"
#include "update_engine/policy_manager/random_provider.h"

namespace chromeos_policy_manager {

// Fake implementation of the RandomProvider base class.
class FakeRandomProvider : public RandomProvider {
 protected:
  virtual bool DoInit() {
    seed_.reset(new FakeVariable<uint64_t>("random_seed"));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeRandomProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_FAKE_RANDOM_PROVIDER_H
