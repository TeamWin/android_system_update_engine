// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/evaluation_context.h"

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

namespace chromeos_policy_manager {

EvaluationContext::EvaluationContext(ClockInterface* clock)
    : clock_(clock),
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
  CancelMainLoopEvent(poll_timeout_event_);
  poll_timeout_event_ = kEventIdNull;
}

TimeDelta EvaluationContext::RemainingTime() const {
  return evaluation_monotonic_deadline_ - clock_->GetMonotonicTime();
}

void EvaluationContext::ValueChanged(BaseVariable* var) {
  DLOG(INFO) << "ValueChanged for variable " << var->GetName();
  OnValueChangedOrPollTimeout();
}

void EvaluationContext::OnPollTimeout() {
  DLOG(INFO) << "OnPollTimeout() called.";
  poll_timeout_event_ = kEventIdNull;
  OnValueChangedOrPollTimeout();
}

void EvaluationContext::OnValueChangedOrPollTimeout() {
  RemoveObserversAndTimeout();

  if (value_changed_callback_.get() != NULL) {
    value_changed_callback_->Run();
    value_changed_callback_.reset();
  }
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
  evaluation_monotonic_deadline_ =
      clock_->GetMonotonicTime() + evaluation_timeout_;
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
  TimeDelta reeval_timeout;
  bool reeval_timeout_set = false;
  bool waiting_for_value_change = false;

  // Check if a reevaluation should be triggered due to a IsTimeGreaterThan()
  // call.
  if (reevaluation_time_ != Time::Max()) {
    reeval_timeout = reevaluation_time_ - evaluation_start_;
    reeval_timeout_set = true;
  }

  if (value_changed_callback_.get() != NULL) {
    LOG(ERROR) << "RunOnValueChangeOrTimeout called more than once.";
    return false;
  }

  for (auto& it : value_cache_) {
    switch (it.first->GetMode()) {
      case kVariableModeAsync:
        waiting_for_value_change = true;
        DLOG(INFO) << "Waiting for value on " << it.first->GetName();
        it.first->AddObserver(this);
        break;
      case kVariableModePoll:
        if (!reeval_timeout_set || reeval_timeout > it.first->GetPollInterval())
          reeval_timeout = it.first->GetPollInterval();
        reeval_timeout_set = true;
        break;
      case kVariableModeConst:
        // Ignored.
        break;
    }
  }
  // Check if the re-evaluation is actually being scheduled. If there are no
  // events waited for, this function should return false.
  if (!waiting_for_value_change && !reeval_timeout_set)
    return false;
  if (reeval_timeout_set) {
    poll_timeout_event_ = RunFromMainLoopAfterTimeout(
        base::Bind(&EvaluationContext::OnPollTimeout,
                   weak_ptr_factory_.GetWeakPtr()),
        reeval_timeout);
  }

  value_changed_callback_.reset(new Closure(callback));
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

}  // namespace chromeos_policy_manager
