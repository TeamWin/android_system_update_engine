// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_UPDATER_PROVIDER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_UPDATER_PROVIDER_H_

#include <string>

#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>

#include "update_engine/policy_manager/provider.h"
#include "update_engine/policy_manager/variable.h"

namespace chromeos_policy_manager {

enum class Stage {
  kIdle,
  kCheckingForUpdate,
  kUpdateAvailable,
  kDownloading,
  kVerifying,
  kFinalizing,
  kUpdatedNeedReboot,
  kReportingErrorEvent,
  kAttemptingRollback,
};

// Provider for Chrome OS update related information.
class UpdaterProvider : public Provider {
 public:
  // A variable returning the last update check time.
  virtual Variable<base::Time>* var_last_checked_time() = 0;

  // A variable reporting the time when an update was last completed in the
  // current boot cycle. Returns an error if an update completed time could not
  // be read (e.g. no update was completed in the current boot cycle) or is
  // invalid.
  //
  // IMPORTANT: The time reported is not the wallclock time reading at the time
  // of the update, rather it is the point in time when the update completed
  // relative to the current wallclock time reading. Therefore, the gap between
  // the reported value and the current wallclock time is guaranteed to be
  // monotonically increasing.
  virtual Variable<base::Time>* var_update_completed_time() = 0;

  // A variable returning the update progress (0.0 to 1.0).
  virtual Variable<double>* var_progress() = 0;

  // A variable returning the current update status.
  virtual Variable<Stage>* var_stage() = 0;

  // A variable returning the update target version.
  virtual Variable<std::string>* var_new_version() = 0;

  // A variable returning the update payload size.
  virtual Variable<size_t>* var_payload_size() = 0;

  // A variable returning the current channel.
  virtual Variable<std::string>* var_curr_channel() = 0;

  // A variable returning the update target channel.
  virtual Variable<std::string>* var_new_channel() = 0;

  // A variable indicating whether P2P updates are allowed.
  virtual Variable<bool>* var_p2p_enabled() = 0;

  // A variable indicating whether updates are allowed over a cellular network.
  virtual Variable<bool>* var_cellular_enabled() = 0;
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_UPDATER_PROVIDER_H_
