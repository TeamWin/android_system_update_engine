// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_FAKE_UPDATER_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_FAKE_UPDATER_PROVIDER_H_

#include <string>

#include "update_engine/update_manager/fake_variable.h"
#include "update_engine/update_manager/updater_provider.h"

namespace chromeos_update_manager {

// Fake implementation of the UpdaterProvider base class.
class FakeUpdaterProvider : public UpdaterProvider {
 public:
  FakeUpdaterProvider() {}

  virtual FakeVariable<base::Time>* var_updater_started_time() override {
    return &var_updater_started_time_;
  }

  virtual FakeVariable<base::Time>* var_last_checked_time() override {
    return &var_last_checked_time_;
  }

  virtual FakeVariable<base::Time>* var_update_completed_time() override {
    return &var_update_completed_time_;
  }

  virtual FakeVariable<double>* var_progress() override {
    return &var_progress_;
  }

  virtual FakeVariable<Stage>* var_stage() override {
    return &var_stage_;
  }

  virtual FakeVariable<std::string>* var_new_version() override {
    return &var_new_version_;
  }

  virtual FakeVariable<int64_t>* var_payload_size() override {
    return &var_payload_size_;
  }

  virtual FakeVariable<std::string>* var_curr_channel() override {
    return &var_curr_channel_;
  }

  virtual FakeVariable<std::string>* var_new_channel() override {
    return &var_new_channel_;
  }

  virtual FakeVariable<bool>* var_p2p_enabled() override {
    return &var_p2p_enabled_;
  }

  virtual FakeVariable<bool>* var_cellular_enabled() override {
    return &var_cellular_enabled_;
  }

  virtual FakeVariable<unsigned int>*
      var_consecutive_failed_update_checks() override {
    return &var_consecutive_failed_update_checks_;
  }

 private:
  FakeVariable<base::Time> var_updater_started_time_{
      "updater_started_time", kVariableModePoll};
  FakeVariable<base::Time> var_last_checked_time_{
      "last_checked_time", kVariableModePoll};
  FakeVariable<base::Time> var_update_completed_time_{
      "update_completed_time", kVariableModePoll};
  FakeVariable<double> var_progress_{"progress", kVariableModePoll};
  FakeVariable<Stage> var_stage_{"stage", kVariableModePoll};
  FakeVariable<std::string> var_new_version_{"new_version", kVariableModePoll};
  FakeVariable<int64_t> var_payload_size_{"payload_size", kVariableModePoll};
  FakeVariable<std::string> var_curr_channel_{
      "curr_channel", kVariableModePoll};
  FakeVariable<std::string> var_new_channel_{"new_channel", kVariableModePoll};
  FakeVariable<bool> var_p2p_enabled_{"p2p_enabled", kVariableModePoll};
  FakeVariable<bool> var_cellular_enabled_{
      "cellular_enabled", kVariableModePoll};
  FakeVariable<unsigned int> var_consecutive_failed_update_checks_{
      "consecutive_failed_update_checks", kVariableModePoll};

  DISALLOW_COPY_AND_ASSIGN(FakeUpdaterProvider);
};

}  // namespace chromeos_update_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_FAKE_UPDATER_PROVIDER_H_
