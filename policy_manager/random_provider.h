// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_RANDOM_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_RANDOM_PROVIDER_H_

#include <base/memory/scoped_ptr.h>

#include "update_engine/policy_manager/provider.h"
#include "update_engine/policy_manager/variable.h"

namespace chromeos_policy_manager {

// Provider of random values.
class RandomProvider : public Provider {
 public:
  // Return a random number every time it is requested. Note that values
  // returned by the variables are cached by the EvaluationContext, so the
  // returned value will be the same during the same policy request. If more
  // random values are needed use a PRNG seeded with this value.
  Variable<uint64_t>* var_seed() const { return var_seed_.get(); }

 protected:
  RandomProvider() {}

  void set_var_seed(Variable<uint64_t>* var_seed) {
    var_seed_.reset(var_seed);
  }

 private:
  // The seed() scoped variable.
  scoped_ptr<Variable<uint64_t>> var_seed_;

  DISALLOW_COPY_AND_ASSIGN(RandomProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_RANDOM_PROVIDER_H_
