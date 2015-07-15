// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_FAKE_UPDATER_PROVIDER_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_FAKE_UPDATER_PROVIDER_H_

#include <string>

#include "update_engine/update_manager/fake_variable.h"
#include "update_engine/update_manager/updater_provider.h"

namespace chromeos_update_manager {

// Fake implementation of the UpdaterProvider base class.
class FakeUpdaterProvider : public UpdaterProvider {
 public:
  FakeUpdaterProvider() {}

  FakeVariable<base::Time>* var_updater_started_time() override {
    return &var_updater_started_time_;
  }

  FakeVariable<base::Time>* var_last_checked_time() override {
    return &var_last_checked_time_;
  }

  FakeVariable<base::Time>* var_update_completed_time() override {
    return &var_update_completed_time_;
  }

  FakeVariable<double>* var_progress() override {
    return &var_progress_;
  }

  FakeVariable<Stage>* var_stage() override {
    return &var_stage_;
  }

  FakeVariable<std::string>* var_new_version() override {
    return &var_new_version_;
  }

  FakeVariable<int64_t>* var_payload_size() override {
    return &var_payload_size_;
  }

  FakeVariable<std::string>* var_curr_channel() override {
    return &var_curr_channel_;
  }

  FakeVariable<std::string>* var_new_channel() override {
    return &var_new_channel_;
  }

  FakeVariable<bool>* var_p2p_enabled() override {
    return &var_p2p_enabled_;
  }

  FakeVariable<bool>* var_cellular_enabled() override {
    return &var_cellular_enabled_;
  }

  FakeVariable<unsigned int>* var_consecutive_failed_update_checks() override {
    return &var_consecutive_failed_update_checks_;
  }

  FakeVariable<unsigned int>* var_server_dictated_poll_interval() override {
    return &var_server_dictated_poll_interval_;
  }

  FakeVariable<UpdateRequestStatus>* var_forced_update_requested() override {
    return &var_forced_update_requested_;
  }

 private:
  FakeVariable<base::Time>
      var_updater_started_time_{  // NOLINT(whitespace/braces)
    "updater_started_time", kVariableModePoll};
  FakeVariable<base::Time> var_last_checked_time_{  // NOLINT(whitespace/braces)
    "last_checked_time", kVariableModePoll};
  FakeVariable<base::Time>
      var_update_completed_time_{  // NOLINT(whitespace/braces)
    "update_completed_time", kVariableModePoll};
  FakeVariable<double> var_progress_{  // NOLINT(whitespace/braces)
    "progress", kVariableModePoll};
  FakeVariable<Stage> var_stage_{  // NOLINT(whitespace/braces)
    "stage", kVariableModePoll};
  FakeVariable<std::string> var_new_version_{  // NOLINT(whitespace/braces)
    "new_version", kVariableModePoll};
  FakeVariable<int64_t> var_payload_size_{  // NOLINT(whitespace/braces)
    "payload_size", kVariableModePoll};
  FakeVariable<std::string> var_curr_channel_{  // NOLINT(whitespace/braces)
    "curr_channel", kVariableModePoll};
  FakeVariable<std::string> var_new_channel_{  // NOLINT(whitespace/braces)
    "new_channel", kVariableModePoll};
  FakeVariable<bool> var_p2p_enabled_{  // NOLINT(whitespace/braces)
    "p2p_enabled", kVariableModePoll};
  FakeVariable<bool> var_cellular_enabled_{  // NOLINT(whitespace/braces)
    "cellular_enabled", kVariableModePoll};
  FakeVariable<unsigned int>
      var_consecutive_failed_update_checks_{  // NOLINT(whitespace/braces)
    "consecutive_failed_update_checks", kVariableModePoll};
  FakeVariable<unsigned int>
      var_server_dictated_poll_interval_{  // NOLINT(whitespace/braces)
    "server_dictated_poll_interval", kVariableModePoll};
  FakeVariable<UpdateRequestStatus>
      var_forced_update_requested_{  // NOLINT(whitespace/braces)
    "forced_update_requested", kVariableModeAsync};

  DISALLOW_COPY_AND_ASSIGN(FakeUpdaterProvider);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_FAKE_UPDATER_PROVIDER_H_
