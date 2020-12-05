//
// Copyright (C) 2014 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/update_manager/real_time_provider.h"

#include <string>

#include <base/time/time.h>

#include "update_engine/common/system_state.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::SystemState;
using std::string;

namespace chromeos_update_manager {

// A variable returning the current date.
class CurrDateVariable : public Variable<Time> {
 public:
  // TODO(garnold) Turn this into an async variable with the needed callback
  // logic for when it value changes.
  explicit CurrDateVariable(const string& name)
      : Variable<Time>(name, TimeDelta::FromHours(1)) {}

 protected:
  virtual const Time* GetValue(TimeDelta /* timeout */, string* /* errmsg */) {
    Time::Exploded now_exp;
    SystemState::Get()->clock()->GetWallclockTime().LocalExplode(&now_exp);
    now_exp.hour = now_exp.minute = now_exp.second = now_exp.millisecond = 0;
    Time* now = new Time();
    bool success = Time::FromLocalExploded(now_exp, now);
    DCHECK(success);
    return now;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CurrDateVariable);
};

// A variable returning the current hour in local time.
class CurrHourVariable : public Variable<int> {
 public:
  // TODO(garnold) Turn this into an async variable with the needed callback
  // logic for when it value changes.
  explicit CurrHourVariable(const string& name)
      : Variable<int>(name, TimeDelta::FromMinutes(5)) {}

 protected:
  virtual const int* GetValue(TimeDelta /* timeout */, string* /* errmsg */) {
    Time::Exploded exploded;
    SystemState::Get()->clock()->GetWallclockTime().LocalExplode(&exploded);
    return new int(exploded.hour);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CurrHourVariable);
};

class CurrMinuteVariable : public Variable<int> {
 public:
  explicit CurrMinuteVariable(const string& name)
      : Variable<int>(name, TimeDelta::FromSeconds(15)) {}

 protected:
  virtual const int* GetValue(TimeDelta /* timeout */, string* /* errmsg */) {
    Time::Exploded exploded;
    SystemState::Get()->clock()->GetWallclockTime().LocalExplode(&exploded);
    return new int(exploded.minute);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CurrMinuteVariable);
};

bool RealTimeProvider::Init() {
  var_curr_date_.reset(new CurrDateVariable("curr_date"));
  var_curr_hour_.reset(new CurrHourVariable("curr_hour"));
  var_curr_minute_.reset(new CurrMinuteVariable("curr_minute"));
  return true;
}

}  // namespace chromeos_update_manager
