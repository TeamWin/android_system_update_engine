// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <chromeos/flag_helper.h>
#include <chromeos/message_loops/glib_message_loop.h>
#include <glib.h>
#include <metrics/metrics_library.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "update_engine/certificate_checker.h"
#include "update_engine/dbus_constants.h"
#include "update_engine/dbus_service.h"
#include "update_engine/dbus_wrapper_interface.h"
#include "update_engine/glib_utils.h"
#include "update_engine/real_system_state.h"
#include "update_engine/subprocess.h"
#include "update_engine/terminator.h"
#include "update_engine/update_attempter.h"
extern "C" {
#include "update_engine/org.chromium.UpdateEngineInterface.dbusserver.h"
}
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace {
const int kDBusSystemMaxWaitSeconds = 2 * 60;
}  // namespace

namespace chromeos_update_engine {

namespace {

// Wait for DBus to be ready by attempting to get the system bus up to
// |timeout| time. Returns whether it succeeded to get the bus.
bool WaitForDBusSystem(base::TimeDelta timeout) {
  GError *error = nullptr;
  DBusGConnection *bus = nullptr;
  Clock clock;
  base::Time deadline = clock.GetMonotonicTime() + timeout;

  while (clock.GetMonotonicTime() < deadline) {
    bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
    if (bus)
      return true;
    LOG(WARNING) << "Failed to get system bus, waiting: "
                 << utils::GetAndFreeGError(&error);
    // Wait 1 second.
    sleep(1);
  }
  LOG(ERROR) << "Failed to get system bus after " << timeout.InSeconds()
             << " seconds.";
  return false;
}

void SetupDBusService(UpdateEngineService* service) {
  DBusGConnection *bus;
  DBusGProxy *proxy;
  GError *error = nullptr;

  bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  LOG_IF(FATAL, !bus) << "Failed to get bus: "
                      << utils::GetAndFreeGError(&error);
  proxy = dbus_g_proxy_new_for_name(bus,
                                    DBUS_SERVICE_DBUS,
                                    DBUS_PATH_DBUS,
                                    DBUS_INTERFACE_DBUS);
  guint32 request_name_ret;
  if (!org_freedesktop_DBus_request_name(proxy,
                                         kUpdateEngineServiceName,
                                         0,
                                         &request_name_ret,
                                         &error)) {
    LOG(FATAL) << "Failed to get name: " << utils::GetAndFreeGError(&error);
  }
  if (request_name_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    g_warning("Got result code %u from requesting name", request_name_ret);
    LOG(FATAL) << "Got result code " << request_name_ret
               << " from requesting name, but expected "
               << DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
  }
  dbus_g_connection_register_g_object(bus,
                                      "/org/chromium/UpdateEngine",
                                      G_OBJECT(service));
}

void SetupLogSymlink(const string& symlink_path, const string& log_path) {
  // TODO(petkov): To ensure a smooth transition between non-timestamped and
  // timestamped logs, move an existing log to start the first timestamped
  // one. This code can go away once all clients are switched to this version or
  // we stop caring about the old-style logs.
  if (utils::FileExists(symlink_path.c_str()) &&
      !utils::IsSymlink(symlink_path.c_str())) {
    base::ReplaceFile(base::FilePath(symlink_path),
                      base::FilePath(log_path),
                      nullptr);
  }
  base::DeleteFile(base::FilePath(symlink_path), true);
  if (symlink(log_path.c_str(), symlink_path.c_str()) == -1) {
    PLOG(ERROR) << "Unable to create symlink " << symlink_path
                << " pointing at " << log_path;
  }
}

string GetTimeAsString(time_t utime) {
  struct tm tm;
  CHECK_EQ(localtime_r(&utime, &tm), &tm);
  char str[16];
  CHECK_EQ(strftime(str, sizeof(str), "%Y%m%d-%H%M%S", &tm), 15u);
  return str;
}

string SetupLogFile(const string& kLogsRoot) {
  const string kLogSymlink = kLogsRoot + "/update_engine.log";
  const string kLogsDir = kLogsRoot + "/update_engine";
  const string kLogPath =
      base::StringPrintf("%s/update_engine.%s",
                         kLogsDir.c_str(),
                         GetTimeAsString(::time(nullptr)).c_str());
  mkdir(kLogsDir.c_str(), 0755);
  SetupLogSymlink(kLogSymlink, kLogPath);
  return kLogSymlink;
}

void SetupLogging(bool log_to_std_err) {
  string log_file;
  logging::LoggingSettings log_settings;
  log_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  log_settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;

  if (log_to_std_err) {
    // Log to stderr initially.
    log_settings.log_file = nullptr;
    log_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  } else {
    log_file = SetupLogFile("/var/log");
    log_settings.log_file = log_file.c_str();
    log_settings.logging_dest = logging::LOG_TO_FILE;
  }

  logging::InitLogging(log_settings);
}

}  // namespace
}  // namespace chromeos_update_engine

int main(int argc, char** argv) {
  DEFINE_bool(logtostderr, false,
              "Write logs to stderr instead of to a file in log_dir.");
  DEFINE_bool(foreground, false,
              "Don't daemon()ize; run in foreground.");

  ::g_type_init();
  dbus_threads_init_default();
  base::AtExitManager exit_manager;  // Required for base/rand_util.h.
  chromeos_update_engine::Terminator::Init();
  chromeos::FlagHelper::Init(argc, argv, "Chromium OS Update Engine");
  chromeos_update_engine::SetupLogging(FLAGS_logtostderr);
  if (!FLAGS_foreground)
    PLOG_IF(FATAL, daemon(0, 0) == 1) << "daemon() failed";

  LOG(INFO) << "Chrome OS Update Engine starting";

  // Ensure that all written files have safe permissions.
  // This is a mask, so we _block_ execute for the owner, and ALL
  // permissions for other users.
  // Done _after_ log file creation.
  umask(S_IXUSR | S_IRWXG | S_IRWXO);

  // Create the single Glib main loop. Code accessing directly the glib main
  // loop (such as calling g_timeout_add() or similar functions) will still work
  // since the backend for the message loop is still the Glib main loop.
  // TODO(deymo): Replace this |loop| with one based on libevent once no other
  // code here uses glib directly.
  chromeos::GlibMessageLoop loop;
  loop.SetAsCurrent();

  // The Subprocess class requires a chromeos::MessageLoop setup.
  chromeos_update_engine::Subprocess subprocess;
  subprocess.Init();

  // Wait up to 2 minutes for DBus to be ready.
  LOG_IF(FATAL, !chromeos_update_engine::WaitForDBusSystem(
      base::TimeDelta::FromSeconds(kDBusSystemMaxWaitSeconds)))
      << "Failed to initialize DBus, aborting.";

  chromeos_update_engine::RealSystemState real_system_state;
  LOG_IF(ERROR, !real_system_state.Initialize())
      << "Failed to initialize system state.";
  chromeos_update_engine::UpdateAttempter *update_attempter =
      real_system_state.update_attempter();
  CHECK(update_attempter);

  // Sets static members for the certificate checker.
  chromeos_update_engine::CertificateChecker::set_system_state(
      &real_system_state);
  chromeos_update_engine::OpenSSLWrapper openssl_wrapper;
  chromeos_update_engine::CertificateChecker::set_openssl_wrapper(
      &openssl_wrapper);

  // Create the dbus service object:
  dbus_g_object_type_install_info(UPDATE_ENGINE_TYPE_SERVICE,
                                  &dbus_glib_update_engine_service_object_info);
  UpdateEngineService* service = update_engine_service_new();
  service->system_state_ = &real_system_state;
  update_attempter->set_dbus_service(service);
  chromeos_update_engine::SetupDBusService(service);

  // Initiate update checks.
  update_attempter->ScheduleUpdates();

  // Update boot flags after 45 seconds.
  loop.PostDelayedTask(
      FROM_HERE,
      base::Bind(&chromeos_update_engine::UpdateAttempter::UpdateBootFlags,
                 base::Unretained(update_attempter)),
      base::TimeDelta::FromSeconds(45));

  // Broadcast the update engine status on startup to ensure consistent system
  // state on crashes.
  loop.PostTask(FROM_HERE, base::Bind(
      &chromeos_update_engine::UpdateAttempter::BroadcastStatus,
      base::Unretained(update_attempter)));

  // Run the UpdateEngineStarted() method on |update_attempter|.
  loop.PostTask(FROM_HERE, base::Bind(
      &chromeos_update_engine::UpdateAttempter::UpdateEngineStarted,
      base::Unretained(update_attempter)));

  // Run the main loop until exit time:
  loop.Run();

  // Cleanup:
  update_attempter->set_dbus_service(nullptr);
  g_object_unref(G_OBJECT(service));

  loop.ReleaseFromCurrent();
  LOG(INFO) << "Chrome OS Update Engine terminating";
  return 0;
}
