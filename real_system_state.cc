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

#include "update_engine/boot_control.h"
#include "update_engine/constants.h"
#include "update_engine/hardware.h"
#include "update_engine/update_manager/state_factory.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

RealSystemState::RealSystemState(const scoped_refptr<dbus::Bus>& bus)
    : debugd_proxy_(bus),
      power_manager_proxy_(bus),
      session_manager_proxy_(bus),
      shill_proxy_(bus),
      libcros_proxy_(bus) {
}

bool RealSystemState::Initialize() {
  metrics_lib_.Init();

  boot_control_ = boot_control::CreateBootControl();
  if (!boot_control_)
    return false;

  hardware_ = hardware::CreateHardware();
  if (!hardware_) {
    LOG(ERROR) << "Error intializing the HardwareInterface.";
    return false;
  }

  LOG_IF(INFO, !hardware_->IsNormalBootMode()) << "Booted in dev mode.";
  LOG_IF(INFO, !hardware_->IsOfficialBuild()) << "Booted non-official build.";

  if (!shill_proxy_.Init()) {
    LOG(ERROR) << "Failed to initialize shill proxy.";
    return false;
  }

  // Initialize standard and powerwash-safe prefs.
  base::FilePath non_volatile_path;
  // TODO(deymo): Fall back to in-memory prefs if there's no physical directory
  // available.
  if (!hardware_->GetNonVolatileDirectory(&non_volatile_path)) {
    LOG(ERROR) << "Failed to get a non-volatile directory.";
    return false;
  }
  Prefs* prefs;
  prefs_.reset(prefs = new Prefs());
  if (!prefs->Init(non_volatile_path.Append(kPrefsSubDirectory))) {
    LOG(ERROR) << "Failed to initialize preferences.";
    return false;
  }

  base::FilePath powerwash_safe_path;
  if (!hardware_->GetPowerwashSafeDirectory(&powerwash_safe_path)) {
    // TODO(deymo): Fall-back to in-memory prefs if there's no powerwash-safe
    // directory, or disable powerwash feature.
    powerwash_safe_path = non_volatile_path.Append("powerwash-safe");
    LOG(WARNING) << "No powerwash-safe directory, using non-volatile one.";
  }
  powerwash_safe_prefs_.reset(prefs = new Prefs());
  if (!prefs->Init(
          powerwash_safe_path.Append(kPowerwashSafePrefsSubDirectory))) {
    LOG(ERROR) << "Failed to initialize powerwash preferences.";
    return false;
  }

  // Check the system rebooted marker file.
  std::string boot_id;
  if (utils::GetBootId(&boot_id)) {
    std::string prev_boot_id;
    system_rebooted_ = (!prefs_->GetString(kPrefsBootId, &prev_boot_id) ||
                        prev_boot_id != boot_id);
    prefs_->SetString(kPrefsBootId, boot_id);
  } else {
    LOG(WARNING) << "Couldn't detect the bootid, assuming system was rebooted.";
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
