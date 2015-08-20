//
// Copyright (C) 2010 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef UPDATE_ENGINE_DBUS_CONSTANTS_H_
#define UPDATE_ENGINE_DBUS_CONSTANTS_H_

namespace chromeos_update_engine {

static const char* const kUpdateEngineServiceName = "org.chromium.UpdateEngine";
static const char* const kUpdateEngineServicePath =
    "/org/chromium/UpdateEngine";
static const char* const kUpdateEngineServiceInterface =
    "org.chromium.UpdateEngineInterface";

// Generic UpdateEngine D-Bus error.
static const char* const kUpdateEngineServiceErrorFailed =
    "org.chromium.UpdateEngine.Error.Failed";

// Flags used in the AttemptUpdateWithFlags() D-Bus method.
typedef enum {
  kAttemptUpdateFlagNonInteractive = (1<<0)
} AttemptUpdateFlags;

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DBUS_CONSTANTS_H_
