// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/evaluation_context.h"

#include <base/bind.h>

using base::Closure;
using base::TimeDelta;

namespace chromeos_policy_manager {

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
  // TODO(deymo): Return a timeout based on the elapsed time on the current
  // policy request evaluation.
  return TimeDelta::FromSeconds(1.);
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
  // Remove the cached values of non-const variables
  for (auto it = value_cache_.begin(); it != value_cache_.end(); ) {
    if (it->first->GetMode() == kVariableModeConst) {
      ++it;
    } else {
      it = value_cache_.erase(it);
    }
  }

  if (value_changed_callback_.get() != NULL) {
    value_changed_callback_->Run();
    value_changed_callback_.reset();
  }
}

bool EvaluationContext::RunOnValueChangeOrTimeout(Closure callback) {
  TimeDelta reeval_timeout;
  bool reeval_timeout_set = false;
  bool waiting_for_value_change = false;

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

}  // namespace chromeos_policy_manager
