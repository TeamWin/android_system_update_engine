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

#include "update_engine/daemon.h"

#include <sysexits.h>

#include <base/bind.h>
#include <base/location.h>
#include <base/time/time.h>
#if USE_WEAVE
#include <binderwrapper/binder_wrapper.h>
#endif  // USE_WEAVE
#include <brillo/message_loops/message_loop.h>

#include "update_engine/common/clock.h"
#include "update_engine/update_attempter.h"

using brillo::MessageLoop;

namespace {
const int kDBusSystemMaxWaitSeconds = 2 * 60;
}  // namespace

namespace chromeos_update_engine {

namespace {
// Wait for passed |bus| DBus to be connected by attempting to connect it up to
// |timeout| time. Returns whether the Connect() eventually succeeded.
bool WaitForDBusSystem(dbus::Bus* bus, base::TimeDelta timeout) {
  Clock clock;
  base::Time deadline = clock.GetMonotonicTime() + timeout;

  while (clock.GetMonotonicTime() < deadline) {
    if (bus->Connect())
      return true;
    LOG(WARNING) << "Failed to get system bus, waiting.";
    // Wait 1 second.
    sleep(1);
  }
  LOG(ERROR) << "Failed to get system bus after " << timeout.InSeconds()
             << " seconds.";
  return false;
}
}  // namespace

UpdateEngineDaemon::~UpdateEngineDaemon() {
  UpdateAttempter* update_attempter = real_system_state_->update_attempter();
  // Prevent any DBus communication from UpdateAttempter when shutting down the
  // daemon.
  if (update_attempter)
    update_attempter->set_dbus_adaptor(nullptr);
}

int UpdateEngineDaemon::OnInit() {
  // Register the |subprocess_| singleton with this Daemon as the signal
  // handler.
  subprocess_.Init(this);

  // We use Daemon::OnInit() and not DBusDaemon::OnInit() to gracefully wait for
  // the D-Bus connection for up two minutes to avoid re-spawning the daemon
  // too fast causing thrashing if dbus-daemon is not running.
  int exit_code = Daemon::OnInit();
  if (exit_code != EX_OK)
    return exit_code;

#if USE_WEAVE
  android::BinderWrapper::Create();
  binder_watcher_.Init();
#endif  // USE_WEAVE

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);

  // Wait for DBus to be ready and exit if it doesn't become available after
  // the timeout.
  if (!WaitForDBusSystem(
          bus_.get(),
          base::TimeDelta::FromSeconds(kDBusSystemMaxWaitSeconds))) {
    // TODO(deymo): Make it possible to run update_engine even if dbus-daemon
    // is not running or constantly crashing.
    LOG(ERROR) << "Failed to initialize DBus, aborting.";
    return 1;
  }

  CHECK(bus_->SetUpAsyncOperations());

  // Initialize update engine global state but continue if something fails.
  real_system_state_.reset(new RealSystemState(bus_));
  LOG_IF(ERROR, !real_system_state_->Initialize())
      << "Failed to initialize system state.";
  UpdateAttempter* update_attempter = real_system_state_->update_attempter();
  CHECK(update_attempter);

  // Create the DBus service.
  dbus_adaptor_.reset(new UpdateEngineAdaptor(real_system_state_.get(), bus_));
  update_attempter->set_dbus_adaptor(dbus_adaptor_.get());

  dbus_adaptor_->RegisterAsync(base::Bind(&UpdateEngineDaemon::OnDBusRegistered,
                                          base::Unretained(this)));
  LOG(INFO) << "Waiting for DBus object to be registered.";
  return EX_OK;
}

void UpdateEngineDaemon::OnDBusRegistered(bool succeeded) {
  if (!succeeded) {
    LOG(ERROR) << "Registering the UpdateEngineAdaptor";
    QuitWithExitCode(1);
    return;
  }

  // Take ownership of the service now that everything is initialized. We need
  // to this now and not before to avoid exposing a well known DBus service
  // path that doesn't have the service it is supposed to implement.
  if (!dbus_adaptor_->RequestOwnership()) {
    LOG(ERROR) << "Unable to take ownership of the DBus service, is there "
               << "other update_engine daemon running?";
    QuitWithExitCode(1);
    return;
  }

  // Initiate update checks.
  UpdateAttempter* update_attempter = real_system_state_->update_attempter();
  update_attempter->ScheduleUpdates();

  // Update boot flags after 45 seconds.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&UpdateAttempter::UpdateBootFlags,
                 base::Unretained(update_attempter)),
      base::TimeDelta::FromSeconds(45));

  // Broadcast the update engine status on startup to ensure consistent system
  // state on crashes.
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &UpdateAttempter::BroadcastStatus,
      base::Unretained(update_attempter)));

  // Run the UpdateEngineStarted() method on |update_attempter|.
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &UpdateAttempter::UpdateEngineStarted,
      base::Unretained(update_attempter)));

  LOG(INFO) << "Finished initialization. Now running the loop.";
}

}  // namespace chromeos_update_engine
