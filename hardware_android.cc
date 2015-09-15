//
// Copyright (C) 2015 The Android Open Source Project
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

#include "update_engine/hardware_android.h"

#include <base/files/file_util.h>
#include <chromeos/make_unique_ptr.h>

#include "update_engine/hardware.h"

using std::string;

namespace {

// The stateful directory used by update_engine.
const char kNonVolatileDirectory[] = "/data/misc/update_engine";

}  // namespace

namespace chromeos_update_engine {

namespace hardware {

// Factory defined in hardware.h.
std::unique_ptr<HardwareInterface> CreateHardware() {
  return chromeos::make_unique_ptr(new HardwareAndroid());
}

}  // namespace hardware

bool HardwareAndroid::IsOfficialBuild() const {
  // TODO(deymo): Read the kind of build we are running from the metadata
  // partition.
  LOG(WARNING) << "STUB: Assuming we are not an official build.";
  return false;
}

bool HardwareAndroid::IsNormalBootMode() const {
  // TODO(deymo): Read the kind of build we are running from the metadata
  // partition.
  LOG(WARNING) << "STUB: Assuming we are in dev-mode.";
  return false;
}

bool HardwareAndroid::IsOOBEComplete(base::Time* out_time_of_oobe) const {
  LOG(WARNING) << "STUB: Assuming OOBE is complete.";
  if (out_time_of_oobe)
    *out_time_of_oobe = base::Time();
  return true;
}

string HardwareAndroid::GetHardwareClass() const {
  LOG(WARNING) << "STUB: GetHardwareClass().";
  return "ANDROID";
}

string HardwareAndroid::GetFirmwareVersion() const {
  LOG(WARNING) << "STUB: GetFirmwareVersion().";
  return "0";
}

string HardwareAndroid::GetECVersion() const {
  LOG(WARNING) << "STUB: GetECVersion().";
  return "0";
}

int HardwareAndroid::GetPowerwashCount() const {
  LOG(WARNING) << "STUB: Assuming no factory reset was performed.";
  return 0;
}

bool HardwareAndroid::GetNonVolatileDirectory(base::FilePath* path) const {
  base::FilePath local_path(kNonVolatileDirectory);
  if (!base::PathExists(local_path)) {
    LOG(ERROR) << "Non-volatile directory not found: " << local_path.value();
    return false;
  }
  *path = local_path;
  return true;
}

bool HardwareAndroid::GetPowerwashSafeDirectory(base::FilePath* path) const {
  // On Android, we don't have a directory persisted across powerwash.
  return false;
}

}  // namespace chromeos_update_engine
