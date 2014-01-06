// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PM_RANDOM_VARS_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PM_RANDOM_VARS_H

#include "base/basictypes.h"
#include "policy_manager/variable.h"

namespace chromeos_policy_manager {

// Return a random number every time it is requested. Note that values returned
// by the variables are cached by the EvaluationContext, so the returned value
// will be the same during the same policy request. If more random values are
// needed use a PRNG seeded with this value.
extern Variable<uint64>* var_random_seed;

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PM_RANDOM_VARS_H
