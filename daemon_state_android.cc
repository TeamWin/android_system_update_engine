//
// Copyright (C) 2016 The Android Open Source Project
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

#include "update_engine/daemon_state_android.h"

namespace chromeos_update_engine {

bool DaemonStateAndroid::Initialize() {
  // TODO(deymo): Implement the Android updater state.
  return true;
}

bool DaemonStateAndroid::StartUpdater() {
  return true;
}

void DaemonStateAndroid::AddObserver(ServiceObserverInterface* observer) {
  service_observers_.insert(observer);
}

void DaemonStateAndroid::RemoveObserver(ServiceObserverInterface* observer) {
  service_observers_.erase(observer);
}

ServiceDelegateAndroidInterface* DaemonStateAndroid::service_delegate() {
  // TODO(deymo): Implement a service delegate and return it here.
  return nullptr;
}

}  // namespace chromeos_update_engine
