// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_time_provider.h"

#include <base/time/time.h>

#include "update_engine/clock_interface.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::ClockInterface;
using std::string;

namespace chromeos_policy_manager {

// A variable returning the current date.
class CurrDateVariable : public Variable<Time> {
 public:
  // TODO(garnold) Turn this into an async variable with the needed callback
  // logic for when it value changes.
  CurrDateVariable(const string& name, ClockInterface* clock)
      : Variable<Time>(name, TimeDelta::FromHours(1)), clock_(clock) {}

 protected:
  virtual const Time* GetValue(base::TimeDelta /* timeout */,
                               string* /* errmsg */) {
    Time::Exploded now_exp;
    clock_->GetWallclockTime().LocalExplode(&now_exp);
    now_exp.hour = now_exp.minute = now_exp.second = now_exp.millisecond = 0;
    return new Time(Time::FromLocalExploded(now_exp));
  }

 private:
  ClockInterface* clock_;

  DISALLOW_COPY_AND_ASSIGN(CurrDateVariable);
};

// A variable returning the current hour in local time.
class CurrHourVariable : public Variable<int> {
 public:
  // TODO(garnold) Turn this into an async variable with the needed callback
  // logic for when it value changes.
  CurrHourVariable(const string& name, ClockInterface* clock)
      : Variable<int>(name, TimeDelta::FromMinutes(5)), clock_(clock) {}

 protected:
  virtual const int* GetValue(base::TimeDelta /* timeout */,
                              string* /* errmsg */) {
    Time::Exploded exploded;
    clock_->GetWallclockTime().LocalExplode(&exploded);
    return new int(exploded.hour);
  }

 private:
  ClockInterface* clock_;

  DISALLOW_COPY_AND_ASSIGN(CurrHourVariable);
};

bool RealTimeProvider::DoInit() {
  set_var_curr_date(new CurrDateVariable("curr_date", clock_));
  set_var_curr_hour(new CurrHourVariable("curr_hour", clock_));
  return true;
}

}  // namespace chromeos_policy_manager
