// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PROVIDER_H_

#include <base/basictypes.h>

namespace chromeos_policy_manager {

// Abstract base class for a policy provider.
class Provider {
 public:
  virtual ~Provider() {}

 protected:
  Provider() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Provider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PROVIDER_H_
