// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_CONSTANTS_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_CONSTANTS_H

namespace chromeos_update_engine {

// The name of the marker file used to trigger powerwash when post-install
// completes successfully so that the device is powerwashed on next reboot.
extern const char kPowerwashMarkerFile[];

// The contents of the powerwash marker file.
extern const char kPowerwashCommand[];

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_CONSTANTS_H
