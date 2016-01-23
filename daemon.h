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

#if USE_WEAVE || USE_BINDER
#include <brillo/binder_watcher.h>
#endif  // USE_WEAVE || USE_BINDER
#include <brillo/daemons/daemon.h>
#if USE_DBUS
#include <brillo/dbus/dbus_connection.h>
#endif  // USE_DBUS

#if USE_BINDER
#ifdef __BRILLO__
#include "update_engine/binder_service.h"
#else  // !defined(__BRILLO__)
#include "update_engine/binder_service_android.h"
#endif  // defined(__BRILLO__)
#endif  // USE_BINDER
#include "update_engine/common/subprocess.h"
#if USE_DBUS
#include "update_engine/dbus_service.h"
#endif  // USE_DBUS
#if defined(__BRILLO__) || defined(USE_DBUS)
#include "update_engine/real_system_state.h"
#endif  // defined(__BRILLO__)

namespace chromeos_update_engine {

class UpdateEngineDaemon : public brillo::Daemon {
 public:
  UpdateEngineDaemon() = default;

 protected:
  int OnInit() override;

 private:
#if USE_DBUS
  // Run from the main loop when the |dbus_adaptor_| object is registered. At
  // this point we can request ownership of the DBus service name and continue
  // initialization.
  void OnDBusRegistered(bool succeeded);

  // Main D-Bus connection and service adaptor.
  brillo::DBusConnection dbus_connection_;
  std::unique_ptr<UpdateEngineAdaptor> dbus_adaptor_;
#endif  // USE_DBUS

  // The Subprocess singleton class requires a brillo::MessageLoop in the
  // current thread, so we need to initialize it from this class instead of
  // the main() function.
  Subprocess subprocess_;

#if USE_WEAVE || USE_BINDER
  brillo::BinderWatcher binder_watcher_;
#endif  // USE_WEAVE || USE_BINDER

#if USE_BINDER
#ifdef __BRILLO__
  android::sp<BinderUpdateEngineService> service_;
#else  // !defined(__BRILLO__)
  android::sp<BinderUpdateEngineAndroidService> service_;
#endif  // defined(__BRILLO__)
#endif  // USE_BINDER

#if defined(__BRILLO__) || defined(USE_DBUS)
  // The RealSystemState uses the previous classes so it should be defined last.
  std::unique_ptr<RealSystemState> real_system_state_;
#else  // !defined(__BRILLO__)
  //TODO(deymo): Define non-Brillo state.
#endif  // defined(__BRILLO__)

  DISALLOW_COPY_AND_ASSIGN(UpdateEngineDaemon);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DAEMON_H_
