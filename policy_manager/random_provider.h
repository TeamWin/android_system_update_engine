// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_RANDOM_PROVIDER_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_RANDOM_PROVIDER_H

#include <base/memory/scoped_ptr.h>

#include "policy_manager/provider.h"
#include "policy_manager/variable.h"

namespace chromeos_policy_manager {

// Provider of random values.
class RandomProvider : public Provider {
 public:
  // Return a random number every time it is requested. Note that values
  // returned by the variables are cached by the EvaluationContext, so the
  // returned value will be the same during the same policy request. If more
  // random values are needed use a PRNG seeded with this value.
  Variable<uint64_t>* seed() const { return seed_.get(); }

 protected:
  RandomProvider() {}

  // The seed() scoped variable.
  scoped_ptr<Variable<uint64_t> > seed_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RandomProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_RANDOM_PROVIDER_H
