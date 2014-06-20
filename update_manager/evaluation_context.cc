// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/evaluation_context.h"

#include <algorithm>
#include <string>

#include <base/bind.h>
#include <base/json/json_writer.h>
#include <base/values.h>

#include "update_engine/utils.h"

using base::Closure;
using base::Time;
using base::TimeDelta;
using chromeos_update_engine::ClockInterface;
using std::string;

namespace chromeos_update_manager {

EvaluationContext::EvaluationContext(ClockInterface* clock,
                                     TimeDelta evaluation_timeout,
                                     TimeDelta expiration_timeout)
    : clock_(clock),
      evaluation_timeout_(evaluation_timeout),
      expiration_monotonic_deadline_(MonotonicDeadline(expiration_timeout)),
      weak_ptr_factory_(this) {
  ResetEvaluation();
}

EvaluationContext::~EvaluationContext() {
  RemoveObserversAndTimeout();
}

void EvaluationContext::RemoveObserversAndTimeout() {
  for (auto& it : value_cache_) {
    if (it.first->GetMode() == kVariableModeAsync)
      it.first->RemoveObserver(this);
  }
  CancelMainLoopEvent(timeout_event_);
  timeout_event_ = kEventIdNull;
}

TimeDelta EvaluationContext::RemainingTime(Time monotonic_deadline) const {
  if (monotonic_deadline.is_max())
    return TimeDelta::Max();
  TimeDelta remaining = monotonic_deadline - clock_->GetMonotonicTime();
  return std::max(remaining, TimeDelta());
}

Time EvaluationContext::MonotonicDeadline(TimeDelta timeout) {
  return (timeout.is_max() ? Time::Max() :
          clock_->GetMonotonicTime() + timeout);
}

void EvaluationContext::ValueChanged(BaseVariable* var) {
  DLOG(INFO) << "ValueChanged() called for variable " << var->GetName();
  OnValueChangedOrTimeout();
}

void EvaluationContext::OnTimeout() {
  DLOG(INFO) << "OnTimeout() called due to "
             << (timeout_marks_expiration_ ? "expiration" : "poll interval");
  timeout_event_ = kEventIdNull;
  is_expired_ = timeout_marks_expiration_;
  OnValueChangedOrTimeout();
}

void EvaluationContext::OnValueChangedOrTimeout() {
  RemoveObserversAndTimeout();

  // Copy the callback handle locally, allowing it to be reassigned.
  scoped_ptr<Closure> callback(callback_.release());

  if (callback.get())
    callback->Run();
}

bool EvaluationContext::IsTimeGreaterThan(base::Time timestamp) {
  if (evaluation_start_ > timestamp)
    return true;
  // We need to keep track of these calls to trigger a reevaluation.
  if (reevaluation_time_ > timestamp)
    reevaluation_time_ = timestamp;
  return false;
}

void EvaluationContext::ResetEvaluation() {
  // It is not important if these two values are not in sync. The first value is
  // a reference in time when the evaluation started, to device time-based
  // values for the current evaluation. The second is a deadline for the
  // evaluation which required a monotonic source of time.
  evaluation_start_ = clock_->GetWallclockTime();
  evaluation_monotonic_deadline_ = MonotonicDeadline(evaluation_timeout_);
  reevaluation_time_ = Time::Max();

  // Remove the cached values of non-const variables
  for (auto it = value_cache_.begin(); it != value_cache_.end(); ) {
    if (it->first->GetMode() == kVariableModeConst) {
      ++it;
    } else {
      it = value_cache_.erase(it);
    }
  }
}

bool EvaluationContext::RunOnValueChangeOrTimeout(Closure callback) {
  // Check that the method was not called more than once.
  if (callback_.get() != nullptr) {
    LOG(ERROR) << "RunOnValueChangeOrTimeout called more than once.";
    return false;
  }

  // Check that the context did not yet expire.
  if (is_expired()) {
    LOG(ERROR) << "RunOnValueChangeOrTimeout called on an expired context.";
    return false;
  }

  TimeDelta timeout(TimeDelta::Max());
  bool waiting_for_value_change = false;

  // Handle reevaluation due to a IsTimeGreaterThan() call.
  if (!reevaluation_time_.is_max())
    timeout = reevaluation_time_ - evaluation_start_;

  // Handle reevaluation due to async or poll variables.
  for (auto& it : value_cache_) {
    switch (it.first->GetMode()) {
      case kVariableModeAsync:
        DLOG(INFO) << "Waiting for value on " << it.first->GetName();
        it.first->AddObserver(this);
        waiting_for_value_change = true;
        break;
      case kVariableModePoll:
        timeout = std::min(timeout, it.first->GetPollInterval());
        break;
      case kVariableModeConst:
        // Ignored.
        break;
    }
  }

  // Check if the re-evaluation is actually being scheduled. If there are no
  // events waited for, this function should return false.
  if (!waiting_for_value_change && timeout.is_max())
    return false;

  // Ensure that we take into account the expiration timeout.
  TimeDelta expiration = RemainingTime(expiration_monotonic_deadline_);
  timeout_marks_expiration_ = expiration < timeout;
  if (timeout_marks_expiration_)
    timeout = expiration;

  // Store the reevaluation callback.
  callback_.reset(new Closure(callback));

  // Schedule a timeout event, if one is set.
  if (!timeout.is_max()) {
    DLOG(INFO) << "Waiting for timeout in "
               << chromeos_update_engine::utils::FormatTimeDelta(timeout);
    timeout_event_ = RunFromMainLoopAfterTimeout(
        base::Bind(&EvaluationContext::OnTimeout,
                   weak_ptr_factory_.GetWeakPtr()),
        timeout);
  }

  return true;
}

string EvaluationContext::DumpContext() const {
  base::DictionaryValue* variables = new base::DictionaryValue();
  for (auto& it : value_cache_) {
    variables->SetString(it.first->GetName(), it.second.ToString());
  }

  base::DictionaryValue value;
  value.Set("variables", variables);  // Adopts |variables|.
  value.SetString("evaluation_start",
                  chromeos_update_engine::utils::ToString(evaluation_start_));

  string json_str;
  base::JSONWriter::WriteWithOptions(&value,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json_str);

  return json_str;
}

}  // namespace chromeos_update_manager
