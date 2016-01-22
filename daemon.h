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

#ifndef UPDATE_ENGINE_DAEMON_H_
#define UPDATE_ENGINE_DAEMON_H_

#include <memory>
#include <string>

#if USE_WEAVE
#include <brillo/binder_watcher.h>
#endif  // USE_WEAVE
#include <brillo/daemons/daemon.h>
#include <brillo/dbus/dbus_connection.h>

#include "update_engine/common/subprocess.h"
#include "update_engine/dbus_service.h"
#include "update_engine/real_system_state.h"

namespace chromeos_update_engine {

class UpdateEngineDaemon : public brillo::Daemon {
 public:
  UpdateEngineDaemon() = default;
  ~UpdateEngineDaemon();

 protected:
  int OnInit() override;

 private:
  // Run from the main loop when the |dbus_adaptor_| object is registered. At
  // this point we can request ownership of the DBus service name and continue
  // initialization.
  void OnDBusRegistered(bool succeeded);

  // Main D-Bus connection and service adaptor.
  brillo::DBusConnection dbus_connection_;
  std::unique_ptr<UpdateEngineAdaptor> dbus_adaptor_;

  // The Subprocess singleton class requires a brillo::MessageLoop in the
  // current thread, so we need to initialize it from this class instead of
  // the main() function.
  Subprocess subprocess_;

#if USE_WEAVE
  brillo::BinderWatcher binder_watcher_;
#endif  // USE_WEAVE

  // The RealSystemState uses the previous classes so it should be defined last.
  std::unique_ptr<RealSystemState> real_system_state_;

  DISALLOW_COPY_AND_ASSIGN(UpdateEngineDaemon);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DAEMON_H_
