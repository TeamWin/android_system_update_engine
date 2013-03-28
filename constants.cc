// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/constants.h"

namespace chromeos_update_engine {

const char kPowerwashMarkerFile[] =
  "/mnt/stateful_partition/factory_install_reset";

const char kPowerwashCommand[] = "safe fast\n";
}
