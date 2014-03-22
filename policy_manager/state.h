// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_H_

#include "update_engine/policy_manager/random_provider.h"
#include "update_engine/policy_manager/shill_provider.h"
#include "update_engine/policy_manager/system_provider.h"
#include "update_engine/policy_manager/time_provider.h"

namespace chromeos_policy_manager {

// The State class is an interface to the ensemble of providers. This class
// gives visibility of the state providers to policy implementations.
class State {
 public:
  virtual ~State() {}
  State(RandomProvider* random_provider, ShillProvider* shill_provider,
        SystemProvider* system_provider, TimeProvider* time_provider) :
      random_provider_(random_provider),
      shill_provider_(shill_provider),
      system_provider_(system_provider),
      time_provider_(time_provider) {}

  // These methods return the given provider.
  virtual RandomProvider* random_provider() { return random_provider_.get(); }
  virtual ShillProvider* shill_provider() { return shill_provider_.get(); }
  virtual TimeProvider* time_provider() { return time_provider_.get(); }
  virtual SystemProvider* system_provider() { return system_provider_.get(); }

 private:
  // Instances of the providers.
  scoped_ptr<RandomProvider> random_provider_;
  scoped_ptr<ShillProvider> shill_provider_;
  scoped_ptr<SystemProvider> system_provider_;
  scoped_ptr<TimeProvider> time_provider_;
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_STATE_H_
