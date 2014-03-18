// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_CONSTANTS_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_CONSTANTS_H_

namespace chromeos_update_engine {

static const char* const kUpdateEngineServiceName = "org.chromium.UpdateEngine";
static const char* const kUpdateEngineServicePath =
    "/org/chromium/UpdateEngine";
static const char* const kUpdateEngineServiceInterface =
    "org.chromium.UpdateEngineInterface";

// Flags used in the AttemptUpdateWithFlags() D-Bus method.
typedef enum {
  kAttemptUpdateFlagNonInteractive = (1<<0)
} AttemptUpdateFlags;

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_CONSTANTS_H_
