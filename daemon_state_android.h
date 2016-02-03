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

#ifndef UPDATE_ENGINE_DAEMON_STATE_ANDROID_H_
#define UPDATE_ENGINE_DAEMON_STATE_ANDROID_H_

#include <set>

#include "update_engine/daemon_state_interface.h"
#include "update_engine/service_delegate_android_interface.h"
#include "update_engine/service_observer_interface.h"

namespace chromeos_update_engine {

class DaemonStateAndroid : public DaemonStateInterface {
 public:
  DaemonStateAndroid() = default;
  ~DaemonStateAndroid() override = default;

  bool Initialize();

  // DaemonStateInterface overrides.
  bool StartUpdater() override;
  void AddObserver(ServiceObserverInterface* observer) override;
  void RemoveObserver(ServiceObserverInterface* observer) override;

  const std::set<ServiceObserverInterface*>& service_observers() {
    return service_observers_;
  }

  // Return a pointer to the service delegate.
  ServiceDelegateAndroidInterface* service_delegate();

 protected:
  std::set<ServiceObserverInterface*> service_observers_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DAEMON_STATE_ANDROID_H_
