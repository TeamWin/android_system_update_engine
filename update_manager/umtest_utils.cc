// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/umtest_utils.h"

#include <base/time/time.h>

namespace chromeos_update_manager {

const unsigned UmTestUtils::kDefaultTimeoutInSeconds = 1;

void PrintTo(const EvalStatus& status, ::std::ostream* os) {
  *os << ToString(status);
}

}  // namespace chromeos_update_manager
