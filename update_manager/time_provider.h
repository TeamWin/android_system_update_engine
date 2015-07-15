// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_TIME_PROVIDER_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_TIME_PROVIDER_H_

#include <base/time/time.h>

#include "update_engine/update_manager/provider.h"
#include "update_engine/update_manager/variable.h"

namespace chromeos_update_manager {

// Provider for time related information.
class TimeProvider : public Provider {
 public:
  ~TimeProvider() override {}

  // Returns the current date. The time of day component will be zero.
  virtual Variable<base::Time>* var_curr_date() = 0;

  // Returns the current hour (0 to 23) in local time. The type is int to keep
  // consistent with base::Time.
  virtual Variable<int>* var_curr_hour() = 0;

 protected:
  TimeProvider() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeProvider);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_TIME_PROVIDER_H_
