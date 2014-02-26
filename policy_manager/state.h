// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_H

#include "update_engine/policy_manager/random_provider.h"
#include "update_engine/policy_manager/shill_provider.h"

namespace chromeos_policy_manager {

// The State class is an interface to the ensemble of providers. This class
// gives visibility of the state providers to policy implementations.
class State {
 public:
  virtual ~State() {}

  // Initializes the State instance. Returns whether the initialization
  // succeeded.
  bool Init() {
    return (random_provider_ && random_provider_->Init() &&
            shill_provider_ && shill_provider_->Init());
  }

  // These functions return the given provider.
  RandomProvider* random_provider() { return random_provider_.get(); }
  ShillProvider* shill_provider() { return shill_provider_.get(); }

 protected:
  // Initialize the private scoped_ptr for each provider.
  void set_random_provider(RandomProvider* random_provider) {
    return random_provider_.reset(random_provider);
  }

  void set_shill_provider(ShillProvider* shill_provider) {
    return shill_provider_.reset(shill_provider);
  }

 private:
  // Instances of the providers.
  scoped_ptr<RandomProvider> random_provider_;
  scoped_ptr<ShillProvider> shill_provider_;
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_H
