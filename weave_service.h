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

#ifndef UPDATE_ENGINE_WEAVE_SERVICE_H_
#define UPDATE_ENGINE_WEAVE_SERVICE_H_

#include <memory>

#include <dbus/bus.h>
#include <libweaved/command.h>
#include <libweaved/device.h>

#include "update_engine/weave_service_interface.h"

namespace chromeos_update_engine {

class WeaveService : public WeaveServiceInterface {
 public:
  WeaveService() = default;
  ~WeaveService() override = default;

  bool Init(scoped_refptr<dbus::Bus> bus, DelegateInterface* delegate);

  // WeaveServiceInterface override.
  void UpdateWeaveState() override;

 private:
  // Weave command handlers. These are called from the message loop whenever a
  // command is received and dispatch the synchronous call to the |delegate_|.
  void OnCheckForUpdates(const std::weak_ptr<weaved::Command>& cmd);
  void OnTrackChannel(const std::weak_ptr<weaved::Command>& cmd);

  WeaveServiceInterface::DelegateInterface* delegate_{nullptr};

  std::unique_ptr<weaved::Device> device_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_WEAVE_SERVICE_H_
