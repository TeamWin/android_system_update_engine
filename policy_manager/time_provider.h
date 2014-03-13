// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_TIME_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_TIME_PROVIDER_H_

#include <base/memory/scoped_ptr.h>
#include <base/time.h>

#include "update_engine/policy_manager/provider.h"
#include "update_engine/policy_manager/variable.h"

namespace chromeos_policy_manager {

// Provider for time related information.
class TimeProvider : public Provider {
 public:
  // Returns the current date. The time of day component will be zero.
  Variable<base::Time>* var_curr_date() const {
    return var_curr_date_.get();
  }

  // Returns the current hour (0 to 23) in local time. The type is int to keep
  // consistent with base::Time.
  Variable<int>* var_curr_hour() const {
    return var_curr_hour_.get();
  }

  // TODO(garnold) Implement a method/variable for querying whether a given
  // point in time was reached.

 protected:
  TimeProvider() {}

  void set_var_curr_date(Variable<base::Time>* var_curr_date) {
    var_curr_date_.reset(var_curr_date);
  }

  void set_var_curr_hour(Variable<int>* var_curr_hour) {
    var_curr_hour_.reset(var_curr_hour);
  }

 private:
  scoped_ptr<Variable<base::Time>> var_curr_date_;
  scoped_ptr<Variable<int>> var_curr_hour_;

  DISALLOW_COPY_AND_ASSIGN(TimeProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_TIME_PROVIDER_H_
