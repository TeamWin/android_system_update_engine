// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/file_util.h>
#include <base/time/time.h>

#include "update_engine/constants.h"
#include "update_engine/real_system_state.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

static const char kOOBECompletedMarker[] = "/home/chronos/.oobe_completed";

RealSystemState::RealSystemState()
    : device_policy_(nullptr),
      connection_manager_(this),
      request_params_(this),
      p2p_manager_(),
      system_rebooted_(false) {}

bool RealSystemState::Initialize(bool enable_gpio) {
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

  if (!payload_state_.Initialize(this))
    return false;

  // Initialize the GPIO handler as instructed.
  if (enable_gpio) {
    // A real GPIO handler. Defer GPIO discovery to ensure the udev has ample
    // time to export the devices. Also require that test mode is physically
    // queried at most once and the result cached, for a more consistent update
    // behavior.
    udev_iface_.reset(new StandardUdevInterface());
    file_descriptor_.reset(new EintrSafeFileDescriptor());
    gpio_handler_.reset(new StandardGpioHandler(udev_iface_.get(),
                                                file_descriptor_.get(),
                                                true, true));
  } else {
    // A no-op GPIO handler, always indicating a non-test mode.
    gpio_handler_.reset(new NoopGpioHandler(false));
  }

  // Create the update attempter.
  update_attempter_.reset(new UpdateAttempter(this, &dbus_));

  // All is well. Initialization successful.
  return true;
}

bool RealSystemState::IsOOBEComplete(base::Time* out_time_of_oobe) {
  struct stat statbuf;
  if (stat(kOOBECompletedMarker, &statbuf) != 0) {
    if (errno != ENOENT) {
      PLOG(ERROR) << "Error getting information about "
                  << kOOBECompletedMarker;
    }
    return false;
  }

  if (out_time_of_oobe != NULL)
    *out_time_of_oobe = base::Time::FromTimeT(statbuf.st_mtime);
  return true;
}

}  // namespace chromeos_update_engine
