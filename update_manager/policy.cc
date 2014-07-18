// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/policy.h"

#include <string>

using std::string;

namespace chromeos_update_manager {

string ToString(EvalStatus status) {
  switch (status) {
    case EvalStatus::kFailed:
      return "kFailed";
    case EvalStatus::kSucceeded:
      return "kSucceeded";
    case EvalStatus::kAskMeAgainLater:
      return "kAskMeAgainLater";
  }
  return "Invalid";
}

}  // namespace chromeos_update_manager
