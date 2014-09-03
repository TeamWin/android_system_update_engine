// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_PROVIDER_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_PROVIDER_H_

#include <base/macros.h>

namespace chromeos_update_manager {

// Abstract base class for a policy provider.
class Provider {
 public:
  virtual ~Provider() {}

 protected:
  Provider() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Provider);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_PROVIDER_H_
