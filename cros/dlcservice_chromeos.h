//
// Copyright (C) 2018 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_CROS_DLCSERVICE_CHROMEOS_H_
#define UPDATE_ENGINE_CROS_DLCSERVICE_CHROMEOS_H_

#include <memory>
#include <string>
#include <vector>

#include "update_engine/common/dlcservice_interface.h"

namespace chromeos_update_engine {

// The Chrome OS implementation of the DlcServiceInterface. This interface
// interacts with dlcservice via D-Bus.
class DlcServiceChromeOS : public DlcServiceInterface {
 public:
  DlcServiceChromeOS() = default;
  ~DlcServiceChromeOS() = default;

  // DlcServiceInterface overrides.

  // Will clear the |dlc_ids|, passed to be modified. Clearing by default has
  // the added benefit of avoiding indeterminate behavior in the case that
  // |dlc_ids| wasn't empty to begin which would lead to possible duplicates and
  // cases when error was not checked it's still safe.
  bool GetDlcsToUpdate(std::vector<std::string>* dlc_ids) override;

  // Call into dlcservice for it to mark the DLC IDs as being installed.
  bool InstallCompleted(const std::vector<std::string>& dlc_ids) override;

  // Call into dlcservice for it to mark the DLC IDs as being updated.
  bool UpdateCompleted(const std::vector<std::string>& dlc_ids) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DlcServiceChromeOS);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CROS_DLCSERVICE_CHROMEOS_H_
