// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SYSTEM_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SYSTEM_PROVIDER_H_

#include <string>

#include "update_engine/policy_manager/system_provider.h"

namespace chromeos_policy_manager {

// SystemProvider concrete implementation.
class RealSystemProvider : public SystemProvider {
 public:
  RealSystemProvider() {}

 private:
  virtual bool DoInit() override;

  DISALLOW_COPY_AND_ASSIGN(RealSystemProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_SYSTEM_PROVIDER_H_
