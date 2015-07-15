// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_FAKE_TIME_PROVIDER_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_FAKE_TIME_PROVIDER_H_

#include "update_engine/update_manager/fake_variable.h"
#include "update_engine/update_manager/time_provider.h"

namespace chromeos_update_manager {

// Fake implementation of the TimeProvider base class.
class FakeTimeProvider : public TimeProvider {
 public:
  FakeTimeProvider() {}

  FakeVariable<base::Time>* var_curr_date() override { return &var_curr_date_; }
  FakeVariable<int>* var_curr_hour() override { return &var_curr_hour_; }

 private:
  FakeVariable<base::Time> var_curr_date_{"curr_date", kVariableModePoll};
  FakeVariable<int> var_curr_hour_{"curr_hour", kVariableModePoll};

  DISALLOW_COPY_AND_ASSIGN(FakeTimeProvider);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_FAKE_TIME_PROVIDER_H_
