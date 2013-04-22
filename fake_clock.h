// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_CLOCK_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_CLOCK_H__

#include "update_engine/clock_interface.h"

namespace chromeos_update_engine {

// Implements a clock that can be made to tell any time you want.
class FakeClock : public ClockInterface {
 public:
  FakeClock() {}

  virtual base::Time GetWallclockTime() {
    return wallclock_time_;
  }

  virtual base::Time GetMonotonicTime() {
    return monotonic_time_;
  }

  void SetWallclockTime(const base::Time &time) {
    wallclock_time_ = time;
  }

  void SetMonotonicTime(const base::Time &time) {
    monotonic_time_ = time;
  }

 private:
  base::Time wallclock_time_;
  base::Time monotonic_time_;

  DISALLOW_COPY_AND_ASSIGN(FakeClock);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_CLOCK_H__
