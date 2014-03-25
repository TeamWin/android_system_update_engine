// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_EVALUATION_CONTEXT_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_EVALUATION_CONTEXT_H_

#include <map>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <base/time/time.h>

#include "update_engine/policy_manager/boxed_value.h"
#include "update_engine/policy_manager/event_loop.h"
#include "update_engine/policy_manager/variable.h"

namespace chromeos_policy_manager {

// The EvaluationContext class is the interface between a policy implementation
// and the state. The EvaluationContext tracks the variables used by a policy
// request and caches the returned values, owning those cached values.
class EvaluationContext :
    public base::RefCounted<EvaluationContext>,
    private BaseVariable::ObserverInterface {
 public:
  EvaluationContext() : weak_ptr_factory_(this) {}
  ~EvaluationContext();

  // Returns a pointer to the value returned by the passed variable |var|. The
  // EvaluationContext instance keeps the ownership of the returned object. The
  // returned object is valid during the life of the EvaluationContext, even if
  // the passed Variable changes it.
  //
  // In case of error, a NULL value is returned.
  template<typename T>
  const T* GetValue(Variable<T>* var);

  // Schedules the passed |callback| closure to be called when a cached
  // variable changes its value or a polling interval passes. If none of these
  // events can happen, for example if there's no cached variable, this method
  // returns false.
  //
  // Right before the passed closure is called the EvaluationContext is
  // reseted, removing all the non-const cached values.
  bool RunOnValueChangeOrTimeout(base::Closure callback);

 private:
  // Removes all the Observers and timeout callbacks scheduled by
  // RunOnValueChangeOrTimeout(). This method is idempotent.
  void RemoveObserversAndTimeout();

  // BaseVariable::ObserverInterface override.
  void ValueChanged(BaseVariable* var);

  // Called from the main loop when the scheduled poll timeout has passed.
  void OnPollTimeout();

  // Removes the observers from the used Variables and cancels the poll timeout
  // and executes the scheduled callback, if any.
  void OnValueChangedOrPollTimeout();

  // The remaining time for the current evaluation.
  base::TimeDelta RemainingTime() const;

  // A map to hold the cached values for every variable.
  typedef std::map<BaseVariable*, BoxedValue> ValueCacheMap;

  // The cached values of the called Variables.
  ValueCacheMap value_cache_;

  // A pointer to a copy of the closure passed to RunOnValueChangeOrTimeout().
  scoped_ptr<base::Closure> value_changed_callback_;

  // The EventId returned by the event loop identifying the timeout callback.
  // Used to cancel the timeout callback.
  EventId poll_timeout_event_ = kEventIdNull;

  base::WeakPtrFactory<EvaluationContext> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EvaluationContext);
};

}  // namespace chromeos_policy_manager

// Include the implementation of the template methods.
#include "update_engine/policy_manager/evaluation_context-inl.h"

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_EVALUATION_CONTEXT_H_
