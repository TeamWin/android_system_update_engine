// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PM_PROVIDER_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PM_PROVIDER_H

namespace chromeos_policy_manager {

// Abstract base class for a policy provider.
class Provider {
 public:
  Provider() : is_initialized_(false) {}
  virtual ~Provider() {}

  // Initializes the provider at most once.  Returns true on success.
  bool Init() {
    return is_initialized_ || (is_initialized_ = DoInit());
  }

 protected:
  // Performs the actual initialization. To be implemented by concrete
  // subclasses.
  virtual bool DoInit() = 0;

 private:
  // Whether the provider was already initialized.
  bool is_initialized_;

  DISALLOW_COPY_AND_ASSIGN(Provider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PM_PROVIDER_H
