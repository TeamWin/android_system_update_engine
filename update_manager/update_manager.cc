// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/update_manager.h"

#include "update_engine/update_manager/chromeos_policy.h"
#include "update_engine/update_manager/state.h"

namespace chromeos_update_manager {

UpdateManager::UpdateManager(chromeos_update_engine::ClockInterface* clock,
                             base::TimeDelta evaluation_timeout,
                             base::TimeDelta expiration_timeout, State* state)
      : default_policy_(clock), state_(state), clock_(clock),
        evaluation_timeout_(evaluation_timeout),
        expiration_timeout_(expiration_timeout),
        weak_ptr_factory_(this) {
  // TODO(deymo): Make it possible to replace this policy with a different
  // implementation with a build-time flag.
  policy_.reset(new ChromeOSPolicy());
}

UpdateManager::~UpdateManager() {
  // Remove pending main loop events associated with any of the outstanding
  // evaluation contexts. This will prevent dangling pending events, causing
  // these contexts to be destructed once the repo itself is destructed.
  for (auto& ec : ec_repo_)
    ec->RemoveObserversAndTimeout();
}

void UpdateManager::UnregisterEvalContext(EvaluationContext* ec) {
  if (!ec_repo_.erase(ec)) {
    LOG(ERROR) << "Unregistering an unknown evaluation context, this is a bug.";
  }
}

}  // namespace chromeos_update_manager
