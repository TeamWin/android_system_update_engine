// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/clock.h"

#include <time.h>

namespace chromeos_update_engine {

base::Time Clock::GetWallclockTime() {
  return base::Time::Now();
}

base::Time Clock::GetMonotonicTime() {
  struct timespec now_ts;
  if (clock_gettime(CLOCK_MONOTONIC_RAW, &now_ts) != 0) {
    // Avoid logging this as an error as call-sites may call this very
    // often and we don't want to fill up the disk. Note that this
    // only fails if running on ancient kernels (CLOCK_MONOTONIC_RAW
    // was added in Linux 2.6.28) so it never fails on a ChromeOS
    // device.
    return base::Time();
  }
  struct timeval now_tv;
  now_tv.tv_sec = now_ts.tv_sec;
  now_tv.tv_usec = now_ts.tv_nsec/base::Time::kNanosecondsPerMicrosecond;
  return base::Time::FromTimeVal(now_tv);
}

base::Time Clock::GetBootTime() {
  struct timespec now_ts;
  if (clock_gettime(CLOCK_BOOTTIME, &now_ts) != 0) {
    // Avoid logging this as an error as call-sites may call this very
    // often and we don't want to fill up the disk. Note that this
    // only fails if running on ancient kernels (CLOCK_BOOTTIME was
    // added in Linux 2.6.39) so it never fails on a ChromeOS device.
    return base::Time();
  }
  struct timeval now_tv;
  now_tv.tv_sec = now_ts.tv_sec;
  now_tv.tv_usec = now_ts.tv_nsec/base::Time::kNanosecondsPerMicrosecond;
  return base::Time::FromTimeVal(now_tv);
}

}  // namespace chromeos_update_engine
