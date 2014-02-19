// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/chromeos_policy.h"
#include "update_engine/policy_manager/policy_manager.h"
#include "update_engine/policy_manager/real_random_provider.h"

namespace chromeos_policy_manager {

template <typename T>
bool InitProvider(scoped_ptr<T>* handle_ptr, T* provider) {
   handle_ptr->reset(provider);
   return handle_ptr->get() && (*handle_ptr)->Init();
}

bool PolicyManager::Init() {
  // TODO(deymo): Make it possible to replace this policy with a different
  // implementation with a build-time flag.
  policy_.reset(new ChromeOSPolicy());

  bool result = true;

  // Initialize all the providers.
  result = result && InitProvider<RandomProvider>(&random_,
                                                  new RealRandomProvider());

  return result;
}

}  // namespace chromeos_policy_manager
