// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_EVALUATION_CONTEXT_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_EVALUATION_CONTEXT_H_

#include <map>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <base/time/time.h>

#include "update_engine/clock_interface.h"
#include "update_engine/update_manager/boxed_value.h"
#include "update_engine/update_manager/event_loop.h"
#include "update_engine/update_manager/variable.h"

namespace chromeos_update_manager {

// The EvaluationContext class is the interface between a policy implementation
// and the state. The EvaluationContext tracks the variables used by a policy
// request and caches the returned values, owning those cached values.
// The same EvaluationContext should be re-used for all the evaluations of the
// same policy request (an AsyncPolicyRequest might involve several
// re-evaluations). Each evaluation of the EvaluationContext is run at a given
// point in time, which is used as a reference for the evaluation timeout and
// the time based queries of the policy, such as IsTimeGreaterThan().
//
// Example:
//
//   scoped_refptr<EvaluationContext> ec = new EvaluationContext;
//
//   ...
//   // The following call to ResetEvaluation() is optional. Use it to reset the
//   // evaluation time if the EvaluationContext isn't used right after its
//   // construction.
//   ec->ResetEvaluation();
//   EvalStatus status = policy->SomeMethod(ec, state, &result, args...);
//
//   ...
//   // Run a closure when any of the used async variables changes its value or
//   // the timeout for requery the values happens again.
//   ec->RunOnValueChangeOrTimeout(closure);
//   // If the provided |closure| wants to re-evaluate the policy, it should
//   // call ec->ResetEvaluation() to start a new evaluation.
//
class EvaluationContext :
    public base::RefCounted<EvaluationContext>,
    private BaseVariable::ObserverInterface {
 public:
  explicit EvaluationContext(chromeos_update_engine::ClockInterface* clock,
                             base::TimeDelta evaluation_timeout);
  ~EvaluationContext();

  // Returns a pointer to the value returned by the passed variable |var|. The
  // EvaluationContext instance keeps the ownership of the returned object. The
  // returned object is valid during the life of the evaluation, even if the
  // passed Variable changes it.
  //
  // In case of error, a NULL value is returned.
  template<typename T>
  const T* GetValue(Variable<T>* var);

  // Returns whether the passed |timestamp| is greater than the evaluation
  // time. The |timestamp| value should be in the same scale as the values
  // returned by ClockInterface::GetWallclockTime().
  bool IsTimeGreaterThan(base::Time timestamp);

  // TODO(deymo): Move the following methods to an interface only visible by the
  // UpdateManager class and not the policy implementations.

  // Resets the EvaluationContext to its initial state removing all the
  // non-const cached variables and re-setting the evaluation time. This should
  // be called right before any new evaluation starts.
  void ResetEvaluation();

  // Schedules the passed |callback| closure to be called when a cached
  // variable changes its value or a polling interval passes. If none of these
  // events can happen, for example if there's no cached variable, this method
  // returns false.
  //
  // Right before the passed closure is called the EvaluationContext is
  // reseted, removing all the non-const cached values.
  bool RunOnValueChangeOrTimeout(base::Closure callback);

  // Returns a textual representation of the evaluation context,
  // including the variables and their values. This is intended only
  // to help with debugging and the format may change in the future.
  std::string DumpContext() const;

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

  // Pointer to the mockable clock interface;
  chromeos_update_engine::ClockInterface* clock_;

  // The timestamp when the evaluation of this EvaluationContext started. This
  // value is reset every time ResetEvaluation() is called. The time source
  // used is the ClockInterface::GetWallclockTime().
  base::Time evaluation_start_;

  // The timestamp measured on the GetWallclockTime() scale, when a reevaluation
  // should be triggered due to IsTimeGreaterThan() calls value changes. This
  // timestamp is greater or equal to |evaluation_start_| since it is a
  // timestamp in the future, but it can be lower than the current
  // GetWallclockTime() at some point of the evaluation.
  base::Time reevaluation_time_;

  // The timeout of an evaluation, used to compute the RemainingTime() of an
  // evaluation.
  const base::TimeDelta evaluation_timeout_;

  // The timestamp in the ClockInterface::GetMonotonicTime() scale at which the
  // current evaluation should finish. This is used to compute the
  // RemainingTime().
  base::Time evaluation_monotonic_deadline_;

  base::WeakPtrFactory<EvaluationContext> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EvaluationContext);
};

}  // namespace chromeos_update_manager

// Include the implementation of the template methods.
#include "update_engine/update_manager/evaluation_context-inl.h"

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_EVALUATION_CONTEXT_H_
