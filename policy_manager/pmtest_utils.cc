// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/time/time.h>

#include "update_engine/policy_manager/pmtest_utils.h"

namespace chromeos_policy_manager {

const unsigned PmTestUtils::kDefaultTimeoutInSeconds = 1;

void PrintTo(const EvalStatus& status, ::std::ostream* os) {
  *os << ToString(status);
}

}  // namespace chromeos_policy_manager
