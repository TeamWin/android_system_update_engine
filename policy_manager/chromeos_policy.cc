// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "update_engine/policy_manager/chromeos_policy.h"

using std::string;

namespace chromeos_policy_manager {

EvalStatus ChromeOSPolicy::UpdateCheckAllowed(EvaluationContext* ec,
                                              string* error,
                                              bool* result) const {
  // TODO(deymo): Write this policy implementation with the actual policy.
  *result = true;
  return EvalStatusSucceeded;
}

}  // namespace chromeos_policy_manager
