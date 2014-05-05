// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_UPDATER_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_UPDATER_PROVIDER_H_

#include <base/memory/scoped_ptr.h>

#include "update_engine/policy_manager/generic_variables.h"
#include "update_engine/policy_manager/updater_provider.h"
#include "update_engine/system_state.h"

namespace chromeos_policy_manager {

// A concrete UpdaterProvider implementation using local (in-process) bindings.
class RealUpdaterProvider : public UpdaterProvider {
 public:
  // We assume that any other object handle we get from the system state is
  // "volatile", and so must be re-acquired whenever access is needed; this
  // guarantees that parts of the system state can be mocked out at any time
  // during testing. We further assume that, by the time Init() is called, the
  // system state object is fully populated and usable.
  explicit RealUpdaterProvider(
      chromeos_update_engine::SystemState* system_state);

  // Initializes the provider and returns whether it succeeded.
  bool Init() { return true; }

  virtual Variable<base::Time>* var_updater_started_time() override {
    return &var_updater_started_time_;
  }

  virtual Variable<base::Time>* var_last_checked_time() override {
    return var_last_checked_time_.get();
  }

  virtual Variable<base::Time>* var_update_completed_time() override {
    return var_update_completed_time_.get();
  }

  virtual Variable<double>* var_progress() override {
    return var_progress_.get();
  }

  virtual Variable<Stage>* var_stage() override {
    return var_stage_.get();
  }

  virtual Variable<std::string>* var_new_version() override {
    return var_new_version_.get();
  }

  virtual Variable<int64_t>* var_payload_size() override {
    return var_payload_size_.get();
  }

  virtual Variable<std::string>* var_curr_channel() override {
    return var_curr_channel_.get();
  }

  virtual Variable<std::string>* var_new_channel() override {
    return var_new_channel_.get();
  }

  virtual Variable<bool>* var_p2p_enabled() override {
    return var_p2p_enabled_.get();
  }

  virtual Variable<bool>* var_cellular_enabled() override {
    return var_cellular_enabled_.get();
  }

  virtual Variable<unsigned int>*
      var_consecutive_failed_update_checks() override {
    return var_consecutive_failed_update_checks_.get();
  }

 private:
  // A pointer to the update engine's system state aggregator.
  chromeos_update_engine::SystemState* system_state_;

  // Variable implementations.
  ConstCopyVariable<base::Time> var_updater_started_time_;
  scoped_ptr<Variable<base::Time>> var_last_checked_time_;
  scoped_ptr<Variable<base::Time>> var_update_completed_time_;
  scoped_ptr<Variable<double>> var_progress_;
  scoped_ptr<Variable<Stage>> var_stage_;
  scoped_ptr<Variable<std::string>> var_new_version_;
  scoped_ptr<Variable<int64_t>> var_payload_size_;
  scoped_ptr<Variable<std::string>> var_curr_channel_;
  scoped_ptr<Variable<std::string>> var_new_channel_;
  scoped_ptr<Variable<bool>> var_p2p_enabled_;
  scoped_ptr<Variable<bool>> var_cellular_enabled_;
  scoped_ptr<Variable<unsigned int>> var_consecutive_failed_update_checks_;

  DISALLOW_COPY_AND_ASSIGN(RealUpdaterProvider);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_REAL_UPDATER_PROVIDER_H_
