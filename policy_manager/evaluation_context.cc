// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/evaluation_context.h"

using base::TimeDelta;

namespace chromeos_policy_manager {

TimeDelta EvaluationContext::RemainingTime() const {
  // TODO(deymo): Return a timeout based on the elapsed time on the current
  // policy request evaluation.
  return TimeDelta::FromSeconds(1.);
}

}  // namespace chromeos_policy_manager
