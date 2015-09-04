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

#include "update_engine/boot_control_android.h"

#include <chromeos/make_unique_ptr.h>

#include "update_engine/boot_control.h"

using std::string;

namespace chromeos_update_engine {

namespace boot_control {

// Factory defined in boot_control.h.
std::unique_ptr<BootControlInterface> CreateBootControl() {
  return chromeos::make_unique_ptr(new BootControlAndroid());
}

}  // namespace boot_control

// TODO(deymo): Read the values from the libhardware HAL.

unsigned int BootControlAndroid::GetNumSlots() const {
  return 2;
}

BootControlInterface::Slot BootControlAndroid::GetCurrentSlot() const {
  return 0;
}

bool BootControlAndroid::GetPartitionDevice(const string& partition_name,
                                            unsigned int slot,
                                            string* device) const {
  return false;
}

bool BootControlAndroid::IsSlotBootable(Slot slot) const {
  return false;
}

bool BootControlAndroid::MarkSlotUnbootable(Slot slot) {
  return true;
}

}  // namespace chromeos_update_engine
