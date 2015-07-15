// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_REAL_TIME_PROVIDER_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_REAL_TIME_PROVIDER_H_

#include <memory>

#include <base/time/time.h>

#include "update_engine/clock_interface.h"
#include "update_engine/update_manager/time_provider.h"

namespace chromeos_update_manager {

// TimeProvider concrete implementation.
class RealTimeProvider : public TimeProvider {
 public:
  explicit RealTimeProvider(chromeos_update_engine::ClockInterface* clock)
      : clock_(clock) {}

  // Initializes the provider and returns whether it succeeded.
  bool Init();

  Variable<base::Time>* var_curr_date() override {
    return var_curr_date_.get();
  }

  Variable<int>* var_curr_hour() override {
    return var_curr_hour_.get();
  }

 private:
  // A clock abstraction (fakeable).
  chromeos_update_engine::ClockInterface* const clock_;

  std::unique_ptr<Variable<base::Time>> var_curr_date_;
  std::unique_ptr<Variable<int>> var_curr_hour_;

  DISALLOW_COPY_AND_ASSIGN(RealTimeProvider);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_REAL_TIME_PROVIDER_H_
