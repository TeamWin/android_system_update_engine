// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/real_system_state.h"

#include <base/file_util.h>

#include "update_engine/constants.h"
#include "update_engine/policy_manager/state_factory.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

RealSystemState::RealSystemState()
    : device_policy_(nullptr),
      connection_manager_(this),
      update_attempter_(this, &dbus_),
      request_params_(this),
      system_rebooted_(false) {}

bool RealSystemState::Initialize() {
  metrics_lib_.Init();

  if (!prefs_.Init(base::FilePath(kPrefsDirectory))) {
    LOG(ERROR) << "Failed to initialize preferences.";
    return false;
  }

  if (!powerwash_safe_prefs_.Init(base::FilePath(kPowerwashSafePrefsDir))) {
    LOG(ERROR) << "Failed to initialize powerwash preferences.";
    return false;
  }

  if (!utils::FileExists(kSystemRebootedMarkerFile)) {
    if (!utils::WriteFile(kSystemRebootedMarkerFile, "", 0)) {
      LOG(ERROR) << "Could not create reboot marker file";
      return false;
    }
    system_rebooted_ = true;
  }

  p2p_manager_.reset(P2PManager::Construct(NULL, &prefs_, "cros_au",
                                           kMaxP2PFilesToKeep));

  // Initialize the PolicyManager using the default State Factory.
  chromeos_policy_manager::State* pm_state =
      chromeos_policy_manager::DefaultStateFactory(
          &policy_provider_, &dbus_, this);
  if (!pm_state) {
    LOG(ERROR) << "Failed to initialize the policy manager.";
    return false;
  }
  policy_manager_.reset(
      new chromeos_policy_manager::PolicyManager(&clock_, pm_state));

  if (!payload_state_.Initialize(this)) {
    LOG(ERROR) << "Failed to initialize the payload state object.";
    return false;
  }

  // Initialize the update attempter.
  update_attempter_.Init();

  // All is well. Initialization successful.
  return true;
}

}  // namespace chromeos_update_engine
