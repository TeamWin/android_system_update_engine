// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H_
#define UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H_

#include <stdint.h>

#include <string>

#include <base/basictypes.h>
#include <base/time/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

// This gathers local system information and prepares info used by the
// Omaha request action.

namespace chromeos_update_engine {

// The default "official" Omaha update URL.
extern const char* const kProductionOmahaUrl;

class SystemState;

// This class encapsulates the data Omaha gets for the request, along with
// essential state needed for the processing of the request/response.  The
// strings in this struct should not be XML escaped.
//
// TODO(jaysri): chromium-os:39752 tracks the need to rename this class to
// reflect its lifetime more appropriately.
class OmahaRequestParams {
 public:
  explicit OmahaRequestParams(SystemState* system_state)
      : system_state_(system_state),
        os_platform_(kOsPlatform),
        os_version_(kOsVersion),
        board_app_id_(kAppId),
        canary_app_id_(kAppId),
        delta_okay_(true),
        interactive_(false),
        update_disabled_(false),
        wall_clock_based_wait_enabled_(false),
        update_check_count_wait_enabled_(false),
        min_update_checks_needed_(kDefaultMinUpdateChecks),
        max_update_checks_allowed_(kDefaultMaxUpdateChecks),
        is_powerwash_allowed_(false),
        force_lock_down_(false),
        forced_lock_down_(false),
        use_p2p_for_downloading_(false),
        use_p2p_for_sharing_(false) {
    InitFromLsbValue();
  }

  OmahaRequestParams(SystemState* system_state,
                     const std::string& in_os_platform,
                     const std::string& in_os_version,
                     const std::string& in_os_sp,
                     const std::string& in_os_board,
                     const std::string& in_app_id,
                     const std::string& in_app_version,
                     const std::string& in_app_lang,
                     const std::string& in_target_channel,
                     const std::string& in_hwid,
                     const std::string& in_fw_version,
                     const std::string& in_ec_version,
                     bool in_delta_okay,
                     bool in_interactive,
                     const std::string& in_update_url,
                     bool in_update_disabled,
                     const std::string& in_target_version_prefix,
                     bool in_use_p2p_for_downloading,
                     bool in_use_p2p_for_sharing)
      : system_state_(system_state),
        os_platform_(in_os_platform),
        os_version_(in_os_version),
        os_sp_(in_os_sp),
        os_board_(in_os_board),
        board_app_id_(in_app_id),
        canary_app_id_(in_app_id),
        app_version_(in_app_version),
        app_lang_(in_app_lang),
        current_channel_(in_target_channel),
        target_channel_(in_target_channel),
        hwid_(in_hwid),
        fw_version_(in_fw_version),
        ec_version_(in_ec_version),
        delta_okay_(in_delta_okay),
        interactive_(in_interactive),
        update_url_(in_update_url),
        update_disabled_(in_update_disabled),
        target_version_prefix_(in_target_version_prefix),
        wall_clock_based_wait_enabled_(false),
        update_check_count_wait_enabled_(false),
        min_update_checks_needed_(kDefaultMinUpdateChecks),
        max_update_checks_allowed_(kDefaultMaxUpdateChecks),
        is_powerwash_allowed_(false),
        force_lock_down_(false),
        forced_lock_down_(false),
        use_p2p_for_downloading_(in_use_p2p_for_downloading),
        use_p2p_for_sharing_(in_use_p2p_for_sharing) {}

  // Setters and getters for the various properties.
  inline std::string os_platform() const { return os_platform_; }
  inline std::string os_version() const { return os_version_; }
  inline std::string os_sp() const { return os_sp_; }
  inline std::string os_board() const { return os_board_; }
  inline std::string board_app_id() const { return board_app_id_; }
  inline std::string canary_app_id() const { return canary_app_id_; }
  inline std::string app_lang() const { return app_lang_; }
  inline std::string hwid() const { return hwid_; }
  inline std::string fw_version() const { return fw_version_; }
  inline std::string ec_version() const { return ec_version_; }

  inline void set_app_version(const std::string& version) {
    app_version_ = version;
  }
  inline std::string app_version() const { return app_version_; }

  inline std::string current_channel() const { return current_channel_; }
  inline std::string target_channel() const { return target_channel_; }
  inline std::string download_channel() const { return download_channel_; }

  // Can client accept a delta ?
  inline void set_delta_okay(bool ok) { delta_okay_ = ok; }
  inline bool delta_okay() const { return delta_okay_; }

  // True if this is a user-initiated update check.
  inline void set_interactive(bool interactive) { interactive_ = interactive; }
  inline bool interactive() const { return interactive_; }

  inline void set_update_url(const std::string& url) { update_url_ = url; }
  inline std::string update_url() const { return update_url_; }

  inline void set_update_disabled(bool disabled) {
    update_disabled_ = disabled;
  }
  inline bool update_disabled() const { return update_disabled_; }

  inline void set_target_version_prefix(const std::string& prefix) {
    target_version_prefix_ = prefix;
  }

  inline std::string target_version_prefix() const {
    return target_version_prefix_;
  }

  inline void set_wall_clock_based_wait_enabled(bool enabled) {
    wall_clock_based_wait_enabled_ = enabled;
  }
  inline bool wall_clock_based_wait_enabled() const {
    return wall_clock_based_wait_enabled_;
  }

  inline void set_waiting_period(base::TimeDelta period) {
    waiting_period_ = period;
  }
  base::TimeDelta waiting_period() const { return waiting_period_; }

  inline void set_update_check_count_wait_enabled(bool enabled) {
    update_check_count_wait_enabled_ = enabled;
  }

  inline bool update_check_count_wait_enabled() const {
    return update_check_count_wait_enabled_;
  }

  inline void set_min_update_checks_needed(int64_t min) {
    min_update_checks_needed_ = min;
  }
  inline int64_t min_update_checks_needed() const {
    return min_update_checks_needed_;
  }

  inline void set_max_update_checks_allowed(int64_t max) {
    max_update_checks_allowed_ = max;
  }
  inline int64_t max_update_checks_allowed() const {
    return max_update_checks_allowed_;
  }

  inline void set_use_p2p_for_downloading(bool value) {
    use_p2p_for_downloading_ = value;
  }
  inline bool use_p2p_for_downloading() const {
    return use_p2p_for_downloading_;
  }

  inline void set_use_p2p_for_sharing(bool value) {
    use_p2p_for_sharing_ = value;
  }
  inline bool use_p2p_for_sharing() const {
    return use_p2p_for_sharing_;
  }

  inline void set_p2p_url(const std::string& value) {
    p2p_url_ = value;
  }
  inline std::string p2p_url() const {
    return p2p_url_;
  }

  // True if we're trying to update to a more stable channel.
  // i.e. index(target_channel) > index(current_channel).
  bool to_more_stable_channel() const;

  // Returns the app id corresponding to the current value of the
  // download channel.
  std::string GetAppId() const;

  // Suggested defaults
  static const char* const kAppId;
  static const char* const kOsPlatform;
  static const char* const kOsVersion;
  static const char* const kUpdateUrl;
  static const char* const kUpdateChannelKey;
  static const char* const kIsPowerwashAllowedKey;
  static const int64_t kDefaultMinUpdateChecks = 0;
  static const int64_t kDefaultMaxUpdateChecks = 8;

  // Initializes all the data in the object. Non-empty
  // |in_app_version| or |in_update_url| prevents automatic detection
  // of the parameter. Returns true on success, false otherwise.
  bool Init(const std::string& in_app_version,
            const std::string& in_update_url,
            bool in_interactive);

  // Permanently changes the release channel to |channel|. Performs a
  // powerwash, if required and allowed.
  // Returns true on success, false otherwise. Note: This call will fail if
  // there's a channel change pending already. This is to serialize all the
  // channel changes done by the user in order to avoid having to solve
  // numerous edge cases around ensuring the powerwash happens as intended in
  // all such cases.
  bool SetTargetChannel(const std::string& channel, bool is_powerwash_allowed);

  // Updates the download channel for this particular attempt from the current
  // value of target channel.  This method takes a "snapshot" of the current
  // value of target channel and uses it for all subsequent Omaha requests for
  // this attempt (i.e. initial request as well as download progress/error
  // event requests). The snapshot will be updated only when either this method
  // or Init is called again.
  void UpdateDownloadChannel();

  bool is_powerwash_allowed() const { return is_powerwash_allowed_; }

  // For unit-tests.
  void set_root(const std::string& root);
  void set_current_channel(const std::string& channel) {
    current_channel_ = channel;
  }
  void set_target_channel(const std::string& channel) {
    target_channel_ = channel;
  }

  // Enforce security mode for testing purposes.
  void SetLockDown(bool lock);

 private:
  FRIEND_TEST(OmahaRequestParamsTest, IsValidChannelTest);
  FRIEND_TEST(OmahaRequestParamsTest, ShouldLockDownTest);
  FRIEND_TEST(OmahaRequestParamsTest, ChannelIndexTest);
  FRIEND_TEST(OmahaRequestParamsTest, LsbPreserveTest);
  FRIEND_TEST(OmahaRequestParamsTest, CollectECFWVersionsTest);

  // Use a validator that is a non-static member of this class so that its
  // inputs can be mocked in unit tests (e.g., build type for IsValidChannel).
  typedef bool(
      OmahaRequestParams::*ValueValidator)(  // NOLINT(readability/casting)
      const std::string&) const;

  // Returns true if parameter values should be locked down for security
  // reasons. If this is an official build running in normal boot mode, all
  // values except the release channel are parsed only from the read-only rootfs
  // partition and the channel values are restricted to a pre-approved set.
  bool ShouldLockDown() const;

  // Returns true if |channel| is a valid channel, false otherwise. This method
  // restricts the channel value only if the image is official (see
  // IsOfficialBuild).
  bool IsValidChannel(const std::string& channel) const;

  // Returns the index of the given channel.
  int GetChannelIndex(const std::string& channel) const;

  // Returns True if we should store the fw/ec versions based on our hwid_.
  // Compares hwid to a set of whitelisted prefixes.
  bool CollectECFWVersions() const;

  // These are individual helper methods to initialize the said properties from
  // the LSB value.
  void SetTargetChannelFromLsbValue();
  void SetCurrentChannelFromLsbValue();
  void SetIsPowerwashAllowedFromLsbValue();

  // Initializes the required properties from the LSB value.
  void InitFromLsbValue();

  // Fetches the value for a given key from
  // /mnt/stateful_partition/etc/lsb-release if possible and |stateful_override|
  // is true. Failing that, it looks for the key in /etc/lsb-release. If
  // |validator| is non-NULL, uses it to validate and ignore invalid values.
  std::string GetLsbValue(const std::string& key,
                          const std::string& default_value,
                          ValueValidator validator,
                          bool stateful_override) const;

  // Gets the machine type (e.g. "i686").
  std::string GetMachineType() const;

  // Global system context.
  SystemState* system_state_;

  // Basic properties of the OS and Application that go into the Omaha request.
  std::string os_platform_;
  std::string os_version_;
  std::string os_sp_;
  std::string os_board_;

  // The board app id identifies the app id for the board irrespective of the
  // channel that we're on. The canary app id identifies the app id to be used
  // iff we're in the canary-channel. These values could be different depending
  // on how the release tools are implemented.
  std::string board_app_id_;
  std::string canary_app_id_;

  std::string app_version_;
  std::string app_lang_;

  // The three channel values we deal with.
  // Current channel: is always the channel from /etc/lsb-release. It never
  // changes. It's just read in during initialization.
  std::string current_channel_;

  // Target channel: It starts off with the value of current channel. But if
  // the user changes the channel, then it'll have a different value. If the
  // user changes multiple times, target channel will always contain the most
  // recent change and is updated immediately to the user-selected value even
  // if we're in the middle of a download (as opposed to download channel
  // which gets updated only at the start of next download)
  std::string target_channel_;

  // The channel from which we're downloading the payload. This should normally
  // be the same as target channel. But if the user made another channel change
  // we started the download, then they'd be different, in which case, we'd
  // detect elsewhere that the target channel has been changed and cancel the
  // current download attempt.
  std::string download_channel_;

  std::string hwid_;  // Hardware Qualification ID of the client
  std::string fw_version_;  // Chrome OS Firmware Version.
  std::string ec_version_;  // Chrome OS EC Version.
  bool delta_okay_;  // If this client can accept a delta
  bool interactive_;   // Whether this is a user-initiated update check

  // The URL to send the Omaha request to.
  std::string update_url_;

  // True if we've been told to block updates per enterprise policy.
  bool update_disabled_;

  // Prefix of the target OS version that the enterprise wants this device
  // to be pinned to. It's empty otherwise.
  std::string target_version_prefix_;

  // True if scattering is enabled, in which case waiting_period_ specifies the
  // amount of absolute time that we've to wait for before sending a request to
  // Omaha.
  bool wall_clock_based_wait_enabled_;
  base::TimeDelta waiting_period_;

  // True if scattering is enabled to denote the number of update checks
  // we've to skip before we can send a request to Omaha. The min and max
  // values establish the bounds for a random number to be chosen within that
  // range to enable such a wait.
  bool update_check_count_wait_enabled_;
  int64_t min_update_checks_needed_;
  int64_t max_update_checks_allowed_;

  // True if we are allowed to do powerwash, if required, on a channel change.
  bool is_powerwash_allowed_;

  // When reading files, prepend root_ to the paths. Useful for testing.
  std::string root_;

  // Force security lock down for testing purposes.
  bool force_lock_down_;
  bool forced_lock_down_;

  // True if we may use p2p to download. This is based on owner
  // preferences and policy.
  bool use_p2p_for_downloading_;

  // True if we may use p2p to share. This is based on owner
  // preferences and policy.
  bool use_p2p_for_sharing_;

  // An URL to a local peer serving the requested payload or "" if no
  // such peer is available.
  std::string p2p_url_;

  // TODO(jaysri): Uncomment this after fixing unit tests, as part of
  // chromium-os:39752
  // DISALLOW_COPY_AND_ASSIGN(OmahaRequestParams);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H_
