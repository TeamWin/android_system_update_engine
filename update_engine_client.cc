// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <sysexits.h>
#include <unistd.h>

#include <string>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/macros.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/flag_helper.h>
#include <dbus/bus.h>

#include "update_engine/dbus_constants.h"
#include "update_engine/dbus_proxies.h"

using chromeos_update_engine::kAttemptUpdateFlagNonInteractive;
using chromeos_update_engine::kUpdateEngineServiceName;
using std::string;

namespace {

// Constant to signal that we need to continue running the daemon after
// initialization.
const int kContinueRunning = -1;

class UpdateEngineClient : public chromeos::DBusDaemon {
 public:
  UpdateEngineClient(int argc, char** argv) : argc_(argc), argv_(argv) {}
  ~UpdateEngineClient() override = default;

 protected:
  int OnInit() override {
    int ret = DBusDaemon::OnInit();
    if (ret != EX_OK)
      return ret;
    if (!InitProxy())
      return 1;
    ret = ProcessFlags();
    if (ret != kContinueRunning) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&UpdateEngineClient::QuitWithExitCode,
                     base::Unretained(this), ret));
    }
    return EX_OK;
  }

 private:
  bool InitProxy();

  // Callback called when a StatusUpdate signal is received.
  void OnStatusUpdateSignal(int64_t last_checked_time,
                            double progress,
                            const string& current_operation,
                            const string& new_version,
                            int64_t new_size);
  // Callback called when the OnStatusUpdateSignal() handler is registered.
  void OnStatusUpdateSignalRegistration(const string& interface,
                                        const string& signal_name,
                                        bool success);

  // Registers a callback that prints on stderr the received StatusUpdate
  // signals.
  // The daemon should continue running for this to work.
  void WatchForUpdates();

  void ResetStatus();

  // Show the status of the update engine in stdout.
  // Blocking call. Exits the program with error 1 in case of an error.
  void ShowStatus();

  // Return the current operation status, such as UPDATE_STATUS_IDLE.
  // Blocking call. Exits the program with error 1 in case of an error.
  string GetCurrentOperation();

  void Rollback(bool rollback);
  string GetRollbackPartition();
  string GetKernelDevices();
  void CheckForUpdates(const string& app_version,
                       const string& omaha_url,
                       bool interactive);

  // Reboot the device if a reboot is needed.
  // Blocking call. Ignores failures.
  void RebootIfNeeded();

  // Getter and setter for the target channel. If |get_current_channel| is true,
  // the current channel instead of the target channel will be returned.
  // Blocking call. Exits the program with error 1 in case of an error.
  void SetTargetChannel(const string& target_channel, bool allow_powerwash);
  string GetChannel(bool get_current_channel);

  // Getter and setter for the updates over cellular connections.
  // Blocking call. Exits the program with error 1 in case of an error.
  void SetUpdateOverCellularPermission(bool allowed);
  bool GetUpdateOverCellularPermission();

  // Getter and setter for the updates from P2P permission.
  // Blocking call. Exits the program with error 1 in case of an error.
  void SetP2PUpdatePermission(bool enabled);
  bool GetP2PUpdatePermission();

  // This is similar to watching for updates but rather than registering
  // a signal watch, actively poll the daemon just in case it stops
  // sending notifications.
  void WaitForUpdateComplete();
  void OnUpdateCompleteCheck(int64_t last_checked_time,
                             double progress,
                             const string& current_operation,
                             const string& new_version,
                             int64_t new_size);

  // Blocking call. Exits the program with error 1 in case of an error.
  void ShowPrevVersion();

  // Returns whether the current status is such that a reboot is needed.
  // Blocking call. Exits the program with error 1 in case of an error.
  bool GetIsRebootNeeded();

  // Blocks until a reboot is needed. If the reboot is needed, exits the program
  // with 0. Otherwise it exits the program with 1 if an error occurs before
  // the reboot is needed.
  void WaitForRebootNeeded();
  void OnRebootNeededCheck(int64_t last_checked_time,
                           double progress,
                           const string& current_operation,
                           const string& new_version,
                           int64_t new_size);
  // Callback called when the OnRebootNeededCheck() handler is registered. This
  // is useful to check at this point if the reboot is needed, without loosing
  // any StatusUpdate signals and avoiding the race condition.
  void OnRebootNeededCheckRegistration(const string& interface,
                                       const string& signal_name,
                                       bool success);

  // Main method that parses and triggers all the actions based on the passed
  // flags.
  int ProcessFlags();

  // DBus Proxy to the update_engine daemon object used for all the calls.
  std::unique_ptr<org::chromium::UpdateEngineInterfaceProxy> proxy_;

  // Copy of argc and argv passed to main().
  int argc_;
  char** argv_;

  DISALLOW_COPY_AND_ASSIGN(UpdateEngineClient);
};


bool UpdateEngineClient::InitProxy() {
  proxy_.reset(new org::chromium::UpdateEngineInterfaceProxy(bus_));

  if (!proxy_->GetObjectProxy()) {
    LOG(ERROR) << "Error getting dbus proxy for " << kUpdateEngineServiceName;
    return false;
  }
  return true;
}

void UpdateEngineClient::OnStatusUpdateSignal(
    int64_t last_checked_time,
    double progress,
    const string& current_operation,
    const string& new_version,
    int64_t new_size) {
  LOG(INFO) << "Got status update:";
  LOG(INFO) << "  last_checked_time: " << last_checked_time;
  LOG(INFO) << "  progress: " << progress;
  LOG(INFO) << "  current_operation: " << current_operation;
  LOG(INFO) << "  new_version: " << new_version;
  LOG(INFO) << "  new_size: " << new_size;
}

void UpdateEngineClient::OnStatusUpdateSignalRegistration(
    const string& interface,
    const string& signal_name,
    bool success) {
  VLOG(1) << "OnStatusUpdateSignalRegistration(" << interface << ", "
          << signal_name << ", " << success << ");";
  if (!success) {
    LOG(ERROR) << "Couldn't connect to the " << signal_name << " signal.";
    exit(1);
  }
}

void UpdateEngineClient::WatchForUpdates() {
  proxy_->RegisterStatusUpdateSignalHandler(
      base::Bind(&UpdateEngineClient::OnStatusUpdateSignal,
                 base::Unretained(this)),
      base::Bind(&UpdateEngineClient::OnStatusUpdateSignalRegistration,
                 base::Unretained(this)));
}

void UpdateEngineClient::ResetStatus() {
  bool ret = proxy_->ResetStatus(nullptr);
  CHECK(ret) << "ResetStatus() failed.";
}

void UpdateEngineClient::ShowStatus() {
  int64_t last_checked_time = 0;
  double progress = 0.0;
  string current_op;
  string new_version;
  int64_t new_size = 0;

  bool ret = proxy_->GetStatus(
      &last_checked_time, &progress, &current_op, &new_version, &new_size,
      nullptr);
  CHECK(ret) << "GetStatus() failed";
  printf("LAST_CHECKED_TIME=%" PRIi64 "\nPROGRESS=%f\nCURRENT_OP=%s\n"
         "NEW_VERSION=%s\nNEW_SIZE=%" PRIi64 "\n",
         last_checked_time,
         progress,
         current_op.c_str(),
         new_version.c_str(),
         new_size);
}

string UpdateEngineClient::GetCurrentOperation() {
  int64_t last_checked_time = 0;
  double progress = 0.0;
  string current_op;
  string new_version;
  int64_t new_size = 0;

  bool ret = proxy_->GetStatus(
      &last_checked_time, &progress, &current_op, &new_version, &new_size,
      nullptr);
  CHECK(ret) << "GetStatus() failed";
  return current_op;
}

void UpdateEngineClient::Rollback(bool rollback) {
  bool ret = proxy_->AttemptRollback(rollback, nullptr);
  CHECK(ret) << "Rollback request failed.";
}

string UpdateEngineClient::GetRollbackPartition() {
  string rollback_partition;
  bool ret = proxy_->GetRollbackPartition(&rollback_partition, nullptr);
  CHECK(ret) << "Error while querying rollback partition availabilty.";
  return rollback_partition;
}

string UpdateEngineClient::GetKernelDevices() {
  string kernel_devices;
  bool ret = proxy_->GetKernelDevices(&kernel_devices, nullptr);
  CHECK(ret) << "Error while getting a list of kernel devices";
  return kernel_devices;
}

void UpdateEngineClient::CheckForUpdates(const string& app_version,
                                         const string& omaha_url,
                                         bool interactive) {
  int32_t flags = interactive ? 0 : kAttemptUpdateFlagNonInteractive;
  bool ret = proxy_->AttemptUpdateWithFlags(app_version, omaha_url, flags,
                                            nullptr);
  CHECK(ret) << "Error checking for update.";
}

void UpdateEngineClient::RebootIfNeeded() {
  bool ret = proxy_->RebootIfNeeded(nullptr);
  if (!ret) {
    // Reboot error code doesn't necessarily mean that a reboot
    // failed. For example, D-Bus may be shutdown before we receive the
    // result.
    LOG(INFO) << "RebootIfNeeded() failure ignored.";
  }
}

void UpdateEngineClient::SetTargetChannel(const string& target_channel,
                                          bool allow_powerwash) {
  bool ret = proxy_->SetChannel(target_channel, allow_powerwash, nullptr);
  CHECK(ret) << "Error setting the channel.";
  LOG(INFO) << "Channel permanently set to: " << target_channel;
}

string UpdateEngineClient::GetChannel(bool get_current_channel) {
  string channel;
  bool ret = proxy_->GetChannel(get_current_channel, &channel, nullptr);
  CHECK(ret) << "Error getting the channel.";
  return channel;
}

void UpdateEngineClient::SetUpdateOverCellularPermission(bool allowed) {
  bool ret = proxy_->SetUpdateOverCellularPermission(allowed, nullptr);
  CHECK(ret) << "Error setting the update over cellular setting.";
}

bool UpdateEngineClient::GetUpdateOverCellularPermission() {
  bool allowed;
  bool ret = proxy_->GetUpdateOverCellularPermission(&allowed, nullptr);
  CHECK(ret) << "Error getting the update over cellular setting.";
  return allowed;
}

void UpdateEngineClient::SetP2PUpdatePermission(bool enabled) {
  bool ret = proxy_->SetP2PUpdatePermission(enabled, nullptr);
  CHECK(ret) << "Error setting the peer-to-peer update setting.";
}

bool UpdateEngineClient::GetP2PUpdatePermission() {
  bool enabled;
  bool ret = proxy_->GetP2PUpdatePermission(&enabled, nullptr);
  CHECK(ret) << "Error getting the peer-to-peer update setting.";
  return enabled;
}

void UpdateEngineClient::OnUpdateCompleteCheck(
    int64_t /* last_checked_time */,
    double /* progress */,
    const string& current_operation,
    const string& /* new_version */,
    int64_t /* new_size */) {
  if (current_operation == update_engine::kUpdateStatusIdle) {
    LOG(ERROR) << "Update failed, current operations is " << current_operation;
    exit(1);
  }
  if (current_operation == update_engine::kUpdateStatusUpdatedNeedReboot) {
    LOG(INFO) << "Update succeeded -- reboot needed.";
    exit(0);
  }
}

void UpdateEngineClient::WaitForUpdateComplete() {
  proxy_->RegisterStatusUpdateSignalHandler(
      base::Bind(&UpdateEngineClient::OnUpdateCompleteCheck,
                 base::Unretained(this)),
      base::Bind(&UpdateEngineClient::OnStatusUpdateSignalRegistration,
                 base::Unretained(this)));
}

void UpdateEngineClient::ShowPrevVersion() {
  string prev_version = nullptr;

  bool ret = proxy_->GetPrevVersion(&prev_version, nullptr);;
  if (!ret) {
    LOG(ERROR) << "Error getting previous version.";
  } else {
    LOG(INFO) << "Previous version = " << prev_version;
  }
}

bool UpdateEngineClient::GetIsRebootNeeded() {
  return GetCurrentOperation() == update_engine::kUpdateStatusUpdatedNeedReboot;
}

void UpdateEngineClient::OnRebootNeededCheck(
    int64_t /* last_checked_time */,
    double /* progress */,
    const string& current_operation,
    const string& /* new_version */,
    int64_t /* new_size */) {
  if (current_operation == update_engine::kUpdateStatusUpdatedNeedReboot) {
    LOG(INFO) << "Reboot needed.";
    exit(0);
  }
}

void UpdateEngineClient::OnRebootNeededCheckRegistration(
    const string& interface,
    const string& signal_name,
    bool success) {
  if (GetIsRebootNeeded())
    exit(0);
  if (!success) {
    LOG(ERROR) << "Couldn't connect to the " << signal_name << " signal.";
    exit(1);
  }
}

// Blocks until a reboot is needed. Returns true if waiting succeeded,
// false if an error occurred.
void UpdateEngineClient::WaitForRebootNeeded() {
  proxy_->RegisterStatusUpdateSignalHandler(
      base::Bind(&UpdateEngineClient::OnUpdateCompleteCheck,
                 base::Unretained(this)),
      base::Bind(&UpdateEngineClient::OnStatusUpdateSignalRegistration,
                 base::Unretained(this)));
  if (GetIsRebootNeeded())
    exit(0);
}

int UpdateEngineClient::ProcessFlags() {
  DEFINE_string(app_version, "", "Force the current app version.");
  DEFINE_string(channel, "",
                "Set the target channel. The device will be powerwashed if the "
                "target channel is more stable than the current channel unless "
                "--nopowerwash is specified.");
  DEFINE_bool(check_for_update, false, "Initiate check for updates.");
  DEFINE_bool(follow, false, "Wait for any update operations to complete."
              "Exit status is 0 if the update succeeded, and 1 otherwise.");
  DEFINE_bool(interactive, true, "Mark the update request as interactive.");
  DEFINE_string(omaha_url, "", "The URL of the Omaha update server.");
  DEFINE_string(p2p_update, "",
                "Enables (\"yes\") or disables (\"no\") the peer-to-peer update"
                " sharing.");
  DEFINE_bool(powerwash, true, "When performing rollback or channel change, "
              "do a powerwash or allow it respectively.");
  DEFINE_bool(reboot, false, "Initiate a reboot if needed.");
  DEFINE_bool(is_reboot_needed, false, "Exit status 0 if reboot is needed, "
              "2 if reboot is not needed or 1 if an error occurred.");
  DEFINE_bool(block_until_reboot_is_needed, false, "Blocks until reboot is "
              "needed. Returns non-zero exit status if an error occurred.");
  DEFINE_bool(reset_status, false, "Sets the status in update_engine to idle.");
  DEFINE_bool(rollback, false,
              "Perform a rollback to the previous partition. The device will "
              "be powerwashed unless --nopowerwash is specified.");
  DEFINE_bool(can_rollback, false, "Shows whether rollback partition "
              "is available.");
  DEFINE_bool(show_channel, false, "Show the current and target channels.");
  DEFINE_bool(show_p2p_update, false,
              "Show the current setting for peer-to-peer update sharing.");
  DEFINE_bool(show_update_over_cellular, false,
              "Show the current setting for updates over cellular networks.");
  DEFINE_bool(status, false, "Print the status to stdout.");
  DEFINE_bool(update, false, "Forces an update and waits for it to complete. "
              "Implies --follow.");
  DEFINE_string(update_over_cellular, "",
                "Enables (\"yes\") or disables (\"no\") the updates over "
                "cellular networks.");
  DEFINE_bool(watch_for_updates, false,
              "Listen for status updates and print them to the screen.");
  DEFINE_bool(prev_version, false,
              "Show the previous OS version used before the update reboot.");
  DEFINE_bool(show_kernels, false, "Show the list of kernel patritions and "
              "whether each of them is bootable or not");

  // Boilerplate init commands.
  base::CommandLine::Init(argc_, argv_);
  chromeos::FlagHelper::Init(argc_, argv_, "Chromium OS Update Engine Client");

  // Ensure there are no positional arguments.
  const std::vector<string> positional_args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  if (!positional_args.empty()) {
    LOG(ERROR) << "Found a positional argument '" << positional_args.front()
               << "'. If you want to pass a value to a flag, pass it as "
                  "--flag=value.";
    return 1;
  }

  // Update the status if requested.
  if (FLAGS_reset_status) {
    LOG(INFO) << "Setting Update Engine status to idle ...";
    ResetStatus();
    LOG(INFO) << "ResetStatus succeeded; to undo partition table changes run:\n"
                 "(D=$(rootdev -d) P=$(rootdev -s); cgpt p -i$(($(echo ${P#$D} "
                 "| sed 's/^[^0-9]*//')-1)) $D;)";
  }

  // Changes the current update over cellular network setting.
  if (!FLAGS_update_over_cellular.empty()) {
    bool allowed = FLAGS_update_over_cellular == "yes";
    if (!allowed && FLAGS_update_over_cellular != "no") {
      LOG(ERROR) << "Unknown option: \"" << FLAGS_update_over_cellular
                 << "\". Please specify \"yes\" or \"no\".";
    } else {
      SetUpdateOverCellularPermission(allowed);
    }
  }

  // Show the current update over cellular network setting.
  if (FLAGS_show_update_over_cellular) {
    bool allowed = GetUpdateOverCellularPermission();
    LOG(INFO) << "Current update over cellular network setting: "
              << (allowed ? "ENABLED" : "DISABLED");
  }

  if (!FLAGS_powerwash && !FLAGS_rollback && FLAGS_channel.empty()) {
    LOG(ERROR) << "powerwash flag only makes sense rollback or channel change";
    return 1;
  }

  // Change the P2P enabled setting.
  if (!FLAGS_p2p_update.empty()) {
    bool enabled = FLAGS_p2p_update == "yes";
    if (!enabled && FLAGS_p2p_update != "no") {
      LOG(ERROR) << "Unknown option: \"" << FLAGS_p2p_update
                 << "\". Please specify \"yes\" or \"no\".";
    } else {
      SetP2PUpdatePermission(enabled);
    }
  }

  // Show the rollback availability.
  if (FLAGS_can_rollback) {
    string rollback_partition = GetRollbackPartition();
    bool can_rollback = true;
    if (rollback_partition.empty()) {
      rollback_partition = "UNAVAILABLE";
      can_rollback = false;
    } else {
      rollback_partition = "AVAILABLE: " + rollback_partition;
    }

    LOG(INFO) << "Rollback partition: " << rollback_partition;
    if (!can_rollback) {
      return 1;
    }
  }

  // Show the current P2P enabled setting.
  if (FLAGS_show_p2p_update) {
    bool enabled = GetP2PUpdatePermission();
    LOG(INFO) << "Current update using P2P setting: "
              << (enabled ? "ENABLED" : "DISABLED");
  }

  // First, update the target channel if requested.
  if (!FLAGS_channel.empty())
    SetTargetChannel(FLAGS_channel, FLAGS_powerwash);

  // Show the current and target channels if requested.
  if (FLAGS_show_channel) {
    string current_channel = GetChannel(true);
    LOG(INFO) << "Current Channel: " << current_channel;

    string target_channel = GetChannel(false);
    if (!target_channel.empty())
      LOG(INFO) << "Target Channel (pending update): " << target_channel;
  }

  bool do_update_request = FLAGS_check_for_update | FLAGS_update |
      !FLAGS_app_version.empty() | !FLAGS_omaha_url.empty();
  if (FLAGS_update)
    FLAGS_follow = true;

  if (do_update_request && FLAGS_rollback) {
    LOG(ERROR) << "Incompatible flags specified with rollback."
               << "Rollback should not include update-related flags.";
    return 1;
  }

  if (FLAGS_rollback) {
    LOG(INFO) << "Requesting rollback.";
    Rollback(FLAGS_powerwash);
  }

  // Initiate an update check, if necessary.
  if (do_update_request) {
    LOG_IF(WARNING, FLAGS_reboot) << "-reboot flag ignored.";
    string app_version = FLAGS_app_version;
    if (FLAGS_update && app_version.empty()) {
      app_version = "ForcedUpdate";
      LOG(INFO) << "Forcing an update by setting app_version to ForcedUpdate.";
    }
    LOG(INFO) << "Initiating update check and install.";
    CheckForUpdates(app_version, FLAGS_omaha_url, FLAGS_interactive);
  }

  // These final options are all mutually exclusive with one another.
  if (FLAGS_follow + FLAGS_watch_for_updates + FLAGS_reboot +
      FLAGS_status + FLAGS_is_reboot_needed +
      FLAGS_block_until_reboot_is_needed > 1) {
    LOG(ERROR) << "Multiple exclusive options selected. "
               << "Select only one of --follow, --watch_for_updates, --reboot, "
               << "--is_reboot_needed, --block_until_reboot_is_needed, "
               << "or --status.";
    return 1;
  }

  if (FLAGS_status) {
    LOG(INFO) << "Querying Update Engine status...";
    ShowStatus();
    return 0;
  }

  if (FLAGS_follow) {
    LOG(INFO) << "Waiting for update to complete.";
    WaitForUpdateComplete();
    return kContinueRunning;
  }

  if (FLAGS_watch_for_updates) {
    LOG(INFO) << "Watching for status updates.";
    WatchForUpdates();
    return kContinueRunning;
  }

  if (FLAGS_reboot) {
    LOG(INFO) << "Requesting a reboot...";
    RebootIfNeeded();
    return 0;
  }

  if (FLAGS_prev_version) {
    ShowPrevVersion();
  }

  if (FLAGS_show_kernels) {
    LOG(INFO) << "Kernel partitions:\n"
              << GetKernelDevices();
  }

  if (FLAGS_is_reboot_needed) {
    // In case of error GetIsRebootNeeded() will exit with 1.
    if (GetIsRebootNeeded()) {
      return 0;
    } else {
      return 2;
    }
  }

  if (FLAGS_block_until_reboot_is_needed) {
    WaitForRebootNeeded();
    return kContinueRunning;
  }

  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  UpdateEngineClient client(argc, argv);
  return client.Run();
}
