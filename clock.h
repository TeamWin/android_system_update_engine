// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_CLOCK_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_CLOCK_H_

#include "update_engine/clock_interface.h"

namespace chromeos_update_engine {

// Implements a clock.
class Clock : public ClockInterface {
 public:
  Clock() {}

  virtual base::Time GetWallclockTime();

  virtual base::Time GetMonotonicTime();

  virtual base::Time GetBootTime();

 private:

  DISALLOW_COPY_AND_ASSIGN(Clock);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_CLOCK_H_
