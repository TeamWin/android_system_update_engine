//
// Copyright (C) 2012 The Android Open Source Project
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

#include "update_engine/real_system_state.h"

#include <base/files/file_util.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>

#include "update_engine/boot_control_chromeos.h"
#include "update_engine/constants.h"
#include "update_engine/update_manager/state_factory.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

RealSystemState::RealSystemState(const scoped_refptr<dbus::Bus>& bus)
    : debugd_proxy_(bus, debugd::kDebugdServiceName),
      power_manager_proxy_(bus, power_manager::kPowerManagerServiceName),
      session_manager_proxy_(bus, login_manager::kSessionManagerServiceName),
      shill_proxy_(bus),
      libcros_proxy_(bus) {
}

bool RealSystemState::Initialize() {
  metrics_lib_.Init();

  // TODO(deymo): Initialize BootControl based on the build environment.
  BootControlChromeOS* boot_control_cros = new BootControlChromeOS();
  boot_control_.reset(boot_control_cros);
  if (!boot_control_cros->Init()) {
    LOG(ERROR) << "Ignoring BootControlChromeOS failure. We won't run updates.";
  }

  if (!shill_proxy_.Init()) {
    LOG(ERROR) << "Failed to initialize shill proxy.";
    return false;
  }

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

  // Initialize the Update Manager using the default state factory.
  chromeos_update_manager::State* um_state =
      chromeos_update_manager::DefaultStateFactory(
          &policy_provider_, &shill_proxy_, &session_manager_proxy_, this);
  if (!um_state) {
    LOG(ERROR) << "Failed to initialize the Update Manager.";
    return false;
  }
  update_manager_.reset(
      new chromeos_update_manager::UpdateManager(
          &clock_, base::TimeDelta::FromSeconds(5),
          base::TimeDelta::FromHours(12), um_state));

  // The P2P Manager depends on the Update Manager for its initialization.
  p2p_manager_.reset(P2PManager::Construct(
          nullptr, &clock_, update_manager_.get(), "cros_au",
          kMaxP2PFilesToKeep, base::TimeDelta::FromDays(kMaxP2PFileAgeDays)));

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
