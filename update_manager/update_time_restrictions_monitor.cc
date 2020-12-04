//
// Copyright (C) 2020 The Android Open Source Project
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

#include "update_engine/update_manager/update_time_restrictions_monitor.h"

#include <base/bind.h>
#include <base/time/time.h>

#include "update_engine/common/system_state.h"

using base::TimeDelta;
using brillo::MessageLoop;
using chromeos_update_engine::SystemState;

namespace chromeos_update_manager {

namespace {

const WeeklyTimeInterval* FindNextNearestInterval(
    const WeeklyTimeIntervalVector& intervals, const WeeklyTime& now) {
  const WeeklyTimeInterval* result_interval = nullptr;
  // As we are dealing with weekly time here, the maximum duration can be one
  // week.
  TimeDelta duration_till_next_interval = TimeDelta::FromDays(7);
  for (const auto& interval : intervals) {
    if (interval.InRange(now)) {
      return &interval;
    }
    const TimeDelta current_duration = now.GetDurationTo(interval.start());
    if (current_duration < duration_till_next_interval) {
      result_interval = &interval;
      duration_till_next_interval = current_duration;
    }
  }
  return result_interval;
}

WeeklyTime Now() {
  return WeeklyTime::FromTime(SystemState::Get()->clock()->GetWallclockTime());
}

}  // namespace

UpdateTimeRestrictionsMonitor::UpdateTimeRestrictionsMonitor(
    DevicePolicyProvider* device_policy_provider, Delegate* delegate)
    : evaluation_context_(/* evaluation_timeout = */ TimeDelta::Max(),
                          /* expiration_timeout = */ TimeDelta::Max(),
                          /* unregister_cb = */ {}),
      device_policy_provider_(device_policy_provider),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  if (device_policy_provider_ != nullptr && delegate_ != nullptr)
    StartMonitoring();
}

UpdateTimeRestrictionsMonitor::~UpdateTimeRestrictionsMonitor() {
  StopMonitoring();
}

void UpdateTimeRestrictionsMonitor::StartMonitoring() {
  DCHECK(device_policy_provider_);
  const WeeklyTimeIntervalVector* new_intervals = evaluation_context_.GetValue(
      device_policy_provider_->var_disallowed_time_intervals());
  if (new_intervals && !new_intervals->empty())
    WaitForRestrictedIntervalStarts(*new_intervals);

  const bool is_registered = evaluation_context_.RunOnValueChangeOrTimeout(
      base::Bind(&UpdateTimeRestrictionsMonitor::OnIntervalsChanged,
                 base::Unretained(this)));
  DCHECK(is_registered);
}

void UpdateTimeRestrictionsMonitor::WaitForRestrictedIntervalStarts(
    const WeeklyTimeIntervalVector& restricted_time_intervals) {
  DCHECK(!restricted_time_intervals.empty());

  const WeeklyTimeInterval* current_interval =
      FindNextNearestInterval(restricted_time_intervals, Now());
  if (current_interval == nullptr) {
    LOG(WARNING) << "Could not find next nearest restricted interval.";
    return;
  }

  // If |current_interval| happens right now, set delay to zero.
  const TimeDelta duration_till_start =
      current_interval->InRange(Now())
          ? TimeDelta::FromMicroseconds(0)
          : Now().GetDurationTo(current_interval->start());
  LOG(INFO) << "Found restricted interval starting at "
            << (SystemState::Get()->clock()->GetWallclockTime() +
                duration_till_start);

  timeout_event_ = MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&UpdateTimeRestrictionsMonitor::HandleRestrictedIntervalStarts,
                 weak_ptr_factory_.GetWeakPtr()),
      duration_till_start);
}

void UpdateTimeRestrictionsMonitor::HandleRestrictedIntervalStarts() {
  timeout_event_ = MessageLoop::kTaskIdNull;
  if (delegate_)
    delegate_->OnRestrictedIntervalStarts();
}

void UpdateTimeRestrictionsMonitor::StopMonitoring() {
  MessageLoop::current()->CancelTask(timeout_event_);
  timeout_event_ = MessageLoop::kTaskIdNull;
}

void UpdateTimeRestrictionsMonitor::OnIntervalsChanged() {
  DCHECK(!evaluation_context_.is_expired());

  StopMonitoring();
  evaluation_context_.ResetEvaluation();
  StartMonitoring();
}

}  // namespace chromeos_update_manager
