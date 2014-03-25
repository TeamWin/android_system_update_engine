// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_CLOCK_INTERFACE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_CLOCK_INTERFACE_H_

#include <string>

#include <base/time/time.h>

namespace chromeos_update_engine {

// The clock interface allows access to various system clocks. The
// sole reason for providing this as an interface is unit testing.
// Additionally, the sole reason for the methods not being static
// is also unit testing.
class ClockInterface {
 public:
  // Gets the current time e.g. similar to base::Time::Now().
  virtual base::Time GetWallclockTime() = 0;

  // Returns monotonic time since some unspecified starting point. It
  // is not increased when the system is sleeping nor is it affected
  // by NTP or the user changing the time.
  //
  // (This is a simple wrapper around clock_gettime(2) / CLOCK_MONOTONIC_RAW.)
  virtual base::Time GetMonotonicTime() = 0;

  // Returns monotonic time since some unspecified starting point. It
  // is increased when the system is sleeping but it's not affected
  // by NTP or the user changing the time.
  //
  // (This is a simple wrapper around clock_gettime(2) / CLOCK_BOOTTIME.)
  virtual base::Time GetBootTime() = 0;

  virtual ~ClockInterface() {}
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_CLOCK_INTERFACE_H_
