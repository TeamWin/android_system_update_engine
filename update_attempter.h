// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_ATTEMPTER_H_
#define UPDATE_ENGINE_UPDATE_ATTEMPTER_H_

#include <time.h>

#include <memory>
#include <string>
#include <vector>
#include <utility>

#include <base/time/time.h>
#include <glib.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/action_processor.h"
#include "update_engine/chrome_browser_proxy_resolver.h"
#include "update_engine/download_action.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/proxy_resolver.h"
#include "update_engine/system_state.h"

class MetricsLibraryInterface;
struct UpdateEngineService;

namespace policy {
  class PolicyProvider;
}

namespace chromeos_update_engine {

class DBusWrapperInterface;
class UpdateCheckScheduler;

enum UpdateStatus {
  UPDATE_STATUS_IDLE = 0,
  UPDATE_STATUS_CHECKING_FOR_UPDATE,
  UPDATE_STATUS_UPDATE_AVAILABLE,
  UPDATE_STATUS_DOWNLOADING,
  UPDATE_STATUS_VERIFYING,
  UPDATE_STATUS_FINALIZING,
  UPDATE_STATUS_UPDATED_NEED_REBOOT,
  UPDATE_STATUS_REPORTING_ERROR_EVENT,
  UPDATE_STATUS_ATTEMPTING_ROLLBACK
};

const char* UpdateStatusToString(UpdateStatus status);

class UpdateAttempter : public ActionProcessorDelegate,
                        public DownloadActionDelegate {
 public:
  static const int kMaxDeltaUpdateFailures;

  UpdateAttempter(SystemState* system_state,
                  DBusWrapperInterface* dbus_iface);
  virtual ~UpdateAttempter();

  // Further initialization to be done post construction.
  void Init();

  // Checks for update and, if a newer version is available, attempts to update
  // the system. Non-empty |in_app_version| or |in_update_url| prevents
  // automatic detection of the parameter.  If |obey_proxies| is true, the
  // update will likely respect Chrome's proxy setting. For security reasons, we
  // may still not honor them. |interactive| should be true if this was called
  // from the user (ie dbus).
  virtual void Update(const std::string& app_version,
                      const std::string& omaha_url,
                      bool obey_proxies,
                      bool interactive);

  // ActionProcessorDelegate methods:
  void ProcessingDone(const ActionProcessor* processor, ErrorCode code);
  void ProcessingStopped(const ActionProcessor* processor);
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ErrorCode code);

  // Stop updating. An attempt will be made to record status to the disk
  // so that updates can be resumed later.
  void Terminate();

  // Try to resume from a previously Terminate()d update.
  void ResumeUpdating();

  // Resets the current state to UPDATE_STATUS_IDLE.
  // Used by update_engine_client for restarting a new update without
  // having to reboot once the previous update has reached
  // UPDATE_STATUS_UPDATED_NEED_REBOOT state. This is used only
  // for testing purposes.
  bool ResetStatus();

  // Returns the current status in the out params. Returns true on success.
  virtual bool GetStatus(int64_t* last_checked_time,
                         double* progress,
                         std::string* current_operation,
                         std::string* new_version,
                         int64_t* new_size);

  // Runs chromeos-setgoodkernel, whose responsibility it is to mark the
  // currently booted partition has high priority/permanent/etc. The execution
  // is asynchronous. On completion, the action processor may be started
  // depending on the |start_action_processor_| field. Note that every update
  // attempt goes through this method.
  void UpdateBootFlags();

  // Subprocess::Exec callback.
  void CompleteUpdateBootFlags(int return_code);
  static void StaticCompleteUpdateBootFlags(int return_code,
                                            const std::string& output,
                                            void* p);

  UpdateStatus status() const { return status_; }

  int http_response_code() const { return http_response_code_; }
  void set_http_response_code(int code) { http_response_code_ = code; }

  void set_dbus_service(struct UpdateEngineService* dbus_service) {
    dbus_service_ = dbus_service;
  }

  UpdateCheckScheduler* update_check_scheduler() const {
    return update_check_scheduler_;
  }
  void set_update_check_scheduler(UpdateCheckScheduler* scheduler) {
    update_check_scheduler_ = scheduler;
  }

  // This is the internal entry point for going through an
  // update. If the current status is idle invokes Update.
  // This is called by the DBus implementation.
  void CheckForUpdate(const std::string& app_version,
                      const std::string& omaha_url,
                      bool is_interactive);

  // This is the internal entry point for going through a rollback. This will
  // attempt to run the postinstall on the non-active partition and set it as
  // the partition to boot from. If |powerwash| is True, perform a powerwash
  // as part of rollback. Returns True on success.
  bool Rollback(bool powerwash);

  // This is the internal entry point for checking if we can rollback.
  bool CanRollback() const;

  // This is the internal entry point for getting a rollback partition name,
  // if one exists. It returns the bootable rollback kernel device partition
  // name or empty string if none is available.
  std::string GetRollbackPartition() const;

  // Returns a list of available kernel partitions along with information
  // whether it is possible to boot from it.
  std::vector<std::pair<std::string, bool>> GetKernelDevices() const;

  // Initiates a reboot if the current state is
  // UPDATED_NEED_REBOOT. Returns true on sucess, false otherwise.
  bool RebootIfNeeded();

  // DownloadActionDelegate methods
  void SetDownloadStatus(bool active);
  void BytesReceived(uint64_t bytes_received, uint64_t total);

  // Broadcasts the current status over D-Bus.
  void BroadcastStatus();

  // Returns the special flags to be added to ErrorCode values based on the
  // parameters used in the current update attempt.
  uint32_t GetErrorCodeFlags();

  // Returns true if we should cancel the current download attempt based on the
  // current state of the system, in which case |cancel_reason| indicates the
  // reason for the cancellation.  False otherwise, in which case
  // |cancel_reason| is untouched.
  bool ShouldCancel(ErrorCode* cancel_reason);

  // Called at update_engine startup to do various house-keeping.
  void UpdateEngineStarted();

  // Reloads the device policy from libchromeos. Note: This method doesn't
  // cause a real-time policy fetch from the policy server. It just reloads the
  // latest value that libchromeos has cached. libchromeos fetches the policies
  // from the server asynchronously at its own frequency.
  void RefreshDevicePolicy();

  // Returns the boottime (CLOCK_BOOTTIME) recorded at the last
  // successful update. Returns false if the device has not updated.
  virtual bool GetBootTimeAtUpdate(base::Time *out_boot_time);

  // Returns a version OS version that was being used before the last reboot,
  // and if that reboot happended to be into an update (current version).
  // This will return an empty string otherwise.
  std::string const& GetPrevVersion() const { return prev_version_; }

  // Returns the number of consecutive failed update checks.
  virtual unsigned int consecutive_failed_update_checks() const {
    return consecutive_failed_update_checks_;
  }

  // Returns the poll interval dictated by Omaha, if provided; zero otherwise.
  virtual unsigned int server_dictated_poll_interval() const {
    return server_dictated_poll_interval_;
  }

 private:
  // Update server URL for automated lab test.
  static const char* const kTestUpdateUrl;

  // Special ctor + friend declarations for testing purposes.
  UpdateAttempter(SystemState* system_state,
                  DBusWrapperInterface* dbus_iface,
                  const std::string& update_completed_marker);

  friend class UpdateAttempterUnderTest;
  friend class UpdateAttempterTest;
  FRIEND_TEST(UpdateAttempterTest, ActionCompletedDownloadTest);
  FRIEND_TEST(UpdateAttempterTest, ActionCompletedErrorTest);
  FRIEND_TEST(UpdateAttempterTest, ActionCompletedOmahaRequestTest);
  FRIEND_TEST(UpdateAttempterTest, CreatePendingErrorEventTest);
  FRIEND_TEST(UpdateAttempterTest, CreatePendingErrorEventResumedTest);
  FRIEND_TEST(UpdateAttempterTest, DisableDeltaUpdateIfNeededTest);
  FRIEND_TEST(UpdateAttempterTest, MarkDeltaUpdateFailureTest);
  FRIEND_TEST(UpdateAttempterTest, ReadTrackFromPolicy);
  FRIEND_TEST(UpdateAttempterTest, PingOmahaTest);
  FRIEND_TEST(UpdateAttempterTest, ScheduleErrorEventActionNoEventTest);
  FRIEND_TEST(UpdateAttempterTest, ScheduleErrorEventActionTest);
  FRIEND_TEST(UpdateAttempterTest, UpdateTest);
  FRIEND_TEST(UpdateAttempterTest, ReportDailyMetrics);
  FRIEND_TEST(UpdateAttempterTest, BootTimeInUpdateMarkerFile);

  // Checks if it's more than 24 hours since daily metrics were last
  // reported and, if so, reports daily metrics. Returns |true| if
  // metrics were reported, |false| otherwise.
  bool CheckAndReportDailyMetrics();

  // Calculates and reports the age of the currently running OS. This
  // is defined as the age of the /etc/lsb-release file.
  void ReportOSAge();

  // Sets the status to the given status and notifies a status update over dbus.
  void SetStatusAndNotify(UpdateStatus status);

  // Sets up the download parameters after receiving the update check response.
  void SetupDownload();

  // Creates an error event object in |error_event_| to be included in an
  // OmahaRequestAction once the current action processor is done.
  void CreatePendingErrorEvent(AbstractAction* action, ErrorCode code);

  // If there's a pending error event allocated in |error_event_|, schedules an
  // OmahaRequestAction with that event in the current processor, clears the
  // pending event, updates the status and returns true. Returns false
  // otherwise.
  bool ScheduleErrorEventAction();

  // Sets the cpu shares to |shares| and updates |shares_| if the new
  // |shares| is different than the current |shares_|, otherwise simply
  // returns.
  void SetCpuShares(utils::CpuShares shares);

  // Sets the cpu shares to low and sets up timeout events to increase it.
  void SetupCpuSharesManagement();

  // Resets the cpu shares to normal and destroys any scheduled timeout
  // sources.
  void CleanupCpuSharesManagement();

  // The cpu shares timeout source callback sets the current cpu shares to
  // normal. Returns false so that GLib destroys the timeout source.
  static gboolean StaticManageCpuSharesCallback(gpointer data);
  bool ManageCpuSharesCallback();

  // Callback to start the action processor.
  static gboolean StaticStartProcessing(gpointer data);

  // Schedules an event loop callback to start the action processor. This is
  // scheduled asynchronously to unblock the event loop.
  void ScheduleProcessingStart();

  // Checks if a full update is needed and forces it by updating the Omaha
  // request params.
  void DisableDeltaUpdateIfNeeded();

  // If this was a delta update attempt that failed, count it so that a full
  // update can be tried when needed.
  void MarkDeltaUpdateFailure();

  ProxyResolver* GetProxyResolver() {
    return obeying_proxies_ ?
        reinterpret_cast<ProxyResolver*>(&chrome_proxy_resolver_) :
        reinterpret_cast<ProxyResolver*>(&direct_proxy_resolver_);
  }

  // Sends a ping to Omaha.
  // This is used after an update has been applied and we're waiting for the
  // user to reboot.  This ping helps keep the number of actives count
  // accurate in case a user takes a long time to reboot the device after an
  // update has been applied.
  void PingOmaha();

  // Helper method of Update() to calculate the update-related parameters
  // from various sources and set the appropriate state. Please refer to
  // Update() method for the meaning of the parametes.
  bool CalculateUpdateParams(const std::string& app_version,
                             const std::string& omaha_url,
                             bool obey_proxies,
                             bool interactive);

  // Calculates all the scattering related parameters (such as waiting period,
  // which type of scattering is enabled, etc.) and also updates/deletes
  // the corresponding prefs file used in scattering. Should be called
  // only after the device policy has been loaded and set in the system_state_.
  void CalculateScatteringParams(bool is_interactive);

  // Sets a random value for the waiting period to wait for before downloading
  // an update, if one available. This value will be upperbounded by the
  // scatter factor value specified from policy.
  void GenerateNewWaitingPeriod();

  // Helper method of Update() and Rollback() to construct the sequence of
  // actions to be performed for the postinstall.
  // |previous_action| is the previous action to get
  // bonded with the install_plan that gets passed to postinstall.
  void BuildPostInstallActions(InstallPlanAction* previous_action);

  // Helper method of Update() to construct the sequence of actions to
  // be performed for an update check. Please refer to
  // Update() method for the meaning of the parameters.
  void BuildUpdateActions(bool interactive);

  // Decrements the count in the kUpdateCheckCountFilePath.
  // Returns True if successfully decremented, false otherwise.
  bool DecrementUpdateCheckCount();

  // Starts p2p and performs housekeeping. Returns true only if p2p is
  // running and housekeeping was done.
  bool StartP2PAndPerformHousekeeping();

  // Calculates whether peer-to-peer should be used. Sets the
  // |use_p2p_to_download_| and |use_p2p_to_share_| parameters
  // on the |omaha_request_params_| object.
  void CalculateP2PParams(bool interactive);

  // Starts P2P if it's enabled and there are files to actually share.
  // Called only at program startup. Returns true only if p2p was
  // started and housekeeping was performed.
  bool StartP2PAtStartup();

  // Writes to the processing completed marker. Does nothing if
  // |update_completed_marker_| is empty.
  void WriteUpdateCompletedMarker();

  // Sends a D-Bus message to the Chrome OS power manager asking it to reboot
  // the system. Returns true on success.
  bool RequestPowerManagerReboot();

  // Reboots the system directly by calling /sbin/shutdown. Returns true on
  // success.
  bool RebootDirectly();

  // Last status notification timestamp used for throttling. Use monotonic
  // TimeTicks to ensure that notifications are sent even if the system clock is
  // set back in the middle of an update.
  base::TimeTicks last_notify_time_;

  std::vector<std::shared_ptr<AbstractAction> > actions_;
  scoped_ptr<ActionProcessor> processor_;

  // External state of the system outside the update_engine process
  // carved out separately to mock out easily in unit tests.
  SystemState* system_state_;

  // Interface for getting D-Bus connections.
  DBusWrapperInterface* dbus_iface_ = nullptr;

  // If non-null, this UpdateAttempter will send status updates over this
  // dbus service.
  UpdateEngineService* dbus_service_ = nullptr;

  // Pointer to the OmahaResponseHandlerAction in the actions_ vector.
  std::shared_ptr<OmahaResponseHandlerAction> response_handler_action_;

  // Pointer to the DownloadAction in the actions_ vector.
  std::shared_ptr<DownloadAction> download_action_;

  // Pointer to the preferences store interface. This is just a cached
  // copy of system_state->prefs() because it's used in many methods and
  // is convenient this way.
  PrefsInterface* prefs_ = nullptr;

  // The current UpdateCheckScheduler to notify of state transitions.
  UpdateCheckScheduler* update_check_scheduler_ = nullptr;

  // Pending error event, if any.
  scoped_ptr<OmahaEvent> error_event_;

  // If we should request a reboot even tho we failed the update
  bool fake_update_success_ = false;

  // HTTP server response code from the last HTTP request action.
  int http_response_code_ = 0;

  // Current cpu shares.
  utils::CpuShares shares_ = utils::kCpuSharesNormal;

  // The cpu shares management timeout source.
  GSource* manage_shares_source_ = nullptr;

  // Set to true if an update download is active (and BytesReceived
  // will be called), set to false otherwise.
  bool download_active_ = false;

  // For status:
  UpdateStatus status_;
  double download_progress_ = 0.0;
  int64_t last_checked_time_ = 0;
  std::string prev_version_;
  std::string new_version_ = "0.0.0.0";
  int64_t new_payload_size_ = 0;

  // Common parameters for all Omaha requests.
  OmahaRequestParams* omaha_request_params_ = nullptr;

  // Number of consecutive manual update checks we've had where we obeyed
  // Chrome's proxy settings.
  int proxy_manual_checks_ = 0;

  // If true, this update cycle we are obeying proxies
  bool obeying_proxies_ = true;

  // Our two proxy resolvers
  DirectProxyResolver direct_proxy_resolver_;
  ChromeBrowserProxyResolver chrome_proxy_resolver_;

  // Originally, both of these flags are false. Once UpdateBootFlags is called,
  // |update_boot_flags_running_| is set to true. As soon as UpdateBootFlags
  // completes its asynchronous run, |update_boot_flags_running_| is reset to
  // false and |updated_boot_flags_| is set to true. From that point on there
  // will be no more changes to these flags.
  //
  // True if UpdateBootFlags has completed.
  bool updated_boot_flags_ = false;
  // True if UpdateBootFlags is running.
  bool update_boot_flags_running_ = false;

  // True if the action processor needs to be started by the boot flag updater.
  bool start_action_processor_ = false;

  // Used for fetching information about the device policy.
  scoped_ptr<policy::PolicyProvider> policy_provider_;

  // The current scatter factor as found in the policy setting.
  base::TimeDelta scatter_factor_;

  // Update completed marker file. An empty string means this marker is being
  // ignored (nor is it being written), which is useful for testing situations.
  std::string update_completed_marker_;

  // The number of consecutive failed update checks. Needed for calculating the
  // next update check interval.
  unsigned int consecutive_failed_update_checks_ = 0;

  // The poll interval (in seconds) that was dictated by Omaha, if any; zero
  // otherwise. This is needed for calculating the update check interval.
  unsigned int server_dictated_poll_interval_ = 0;

  DISALLOW_COPY_AND_ASSIGN(UpdateAttempter);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_UPDATE_ATTEMPTER_H_
