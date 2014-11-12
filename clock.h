// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_CLOCK_H_
#define UPDATE_ENGINE_CLOCK_H_

#include "update_engine/clock_interface.h"

namespace chromeos_update_engine {

// Implements a clock.
class Clock : public ClockInterface {
 public:
  Clock() {}

  base::Time GetWallclockTime() override;

  base::Time GetMonotonicTime() override;

  base::Time GetBootTime() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Clock);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CLOCK_H_
