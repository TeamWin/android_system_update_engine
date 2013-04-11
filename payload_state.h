// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_H__

#include <base/time.h>

#include "update_engine/payload_state_interface.h"
#include "update_engine/prefs_interface.h"

namespace chromeos_update_engine {

// Encapsulates all the payload state required for download. This includes the
// state necessary for handling multiple URLs in Omaha response, the backoff
// state, etc. All state is persisted so that we use the most recently saved
// value when resuming the update_engine process. All state is also cached in
// memory so that we ensure we always make progress based on last known good
// state even when there's any issue in reading/writing from the file system.
class PayloadState : public PayloadStateInterface {
 public:
  PayloadState()
      : prefs_(NULL),
        payload_attempt_number_(0),
        url_index_(0),
        url_failure_count_(0) {}

  virtual ~PayloadState() {}

  // Initializes a payload state object using |prefs| for storing the
  // persisted state. It also performs the initial loading of all persisted
  // state into memory and dumps the initial state for debugging purposes.
  // Note: the other methods should be called only after calling Initialize
  // on this object.
  bool Initialize(PrefsInterface* prefs);


  // Implementation of PayloadStateInterface methods.
  virtual void SetResponse(const OmahaResponse& response);
  virtual void DownloadComplete();
  virtual void DownloadProgress(size_t count);
  virtual void UpdateSucceeded();
  virtual void UpdateFailed(ActionExitCode error);
  virtual bool ShouldBackoffDownload();

  virtual inline std::string GetResponseSignature() {
    return response_signature_;
  }

  virtual inline uint32_t GetPayloadAttemptNumber() {
    return payload_attempt_number_;
  }

  virtual inline uint32_t GetUrlIndex() {
    return url_index_;
  }

  virtual inline uint32_t GetUrlFailureCount() {
    return url_failure_count_;
  }

  virtual inline base::Time GetBackoffExpiryTime() {
    return backoff_expiry_time_;
  }

  virtual base::TimeDelta GetUpdateDuration();

  virtual base::TimeDelta GetUpdateDurationUptime();

 private:
  // Increments the payload attempt number which governs the backoff behavior
  // at the time of the next update check.
  void IncrementPayloadAttemptNumber();

  // Advances the current URL index to the next available one. If all URLs have
  // been exhausted during the current payload download attempt (as indicated
  // by the payload attempt number), then it will increment the payload attempt
  // number and wrap around again with the first URL in the list.
  void IncrementUrlIndex();

  // Increments the failure count of the current URL. If the configured max
  // failure count is reached for this URL, it advances the current URL index
  // to the next URL and resets the failure count for that URL.
  void IncrementFailureCount();

  // Updates the backoff expiry time exponentially based on the current
  // payload attempt number.
  void UpdateBackoffExpiryTime();

  // Resets all the persisted state values which are maintained relative to the
  // current response signature. The response signature itself is not reset.
  void ResetPersistedState();

  // Calculates the response "signature", which is basically a string composed
  // of the subset of the fields in the current response that affect the
  // behavior of the PayloadState.
  std::string CalculateResponseSignature();

  // Initializes the current response signature from the persisted state.
  void LoadResponseSignature();

  // Sets the response signature to the given value. Also persists the value
  // being set so that we resume from the save value in case of a process
  // restart.
  void SetResponseSignature(std::string response_signature);

  // Initializes the payload attempt number from the persisted state.
  void LoadPayloadAttemptNumber();

  // Sets the payload attempt number to the given value. Also persists the
  // value being set so that we resume from the same value in case of a process
  // restart.
  void SetPayloadAttemptNumber(uint32_t payload_attempt_number);

  // Initializes the current URL index from the persisted state.
  void LoadUrlIndex();

  // Sets the current URL index to the given value. Also persists the value
  // being set so that we resume from the same value in case of a process
  // restart.
  void SetUrlIndex(uint32_t url_index);

  // Initializes the current URL's failure count from the persisted stae.
  void LoadUrlFailureCount();

  // Sets the current URL's failure count to the given value. Also persists the
  // value being set so that we resume from the same value in case of a process
  // restart.
  void SetUrlFailureCount(uint32_t url_failure_count);

  // Initializes the backoff expiry time from the persisted state.
  void LoadBackoffExpiryTime();

  // Sets the backoff expiry time to the given value. Also persists the value
  // being set so that we resume from the same value in case of a process
  // restart.
  void SetBackoffExpiryTime(const base::Time& new_time);

  // Initializes |update_timestamp_start_| from the persisted state.
  void LoadUpdateTimestampStart();

  // Sets |update_timestamp_start_| to the given value and persists the value.
  void SetUpdateTimestampStart(const base::Time& value);

  // Sets |update_timestamp_end_| to the given value. This is not persisted
  // as it happens at the end of the update process where state is deleted
  // anyway.
  void SetUpdateTimestampEnd(const base::Time& value);

  // Initializes |update_duration_uptime_| from the persisted state.
  void LoadUpdateDurationUptime();

  // Helper method used in SetUpdateDurationUptime() and
  // CalculateUpdateDurationUptime().
  void SetUpdateDurationUptimeExtended(const base::TimeDelta& value,
                                       const base::Time& timestamp,
                                       bool use_logging);

  // Sets |update_duration_uptime_| to the given value and persists
  // the value and sets |update_duration_uptime_timestamp_| to the
  // current monotonic time.
  void SetUpdateDurationUptime(const base::TimeDelta& value);

  // Adds the difference between current monotonic time and
  // |update_duration_uptime_timestamp_| to |update_duration_uptime_| and
  // sets |update_duration_uptime_timestamp_| to current monotonic time.
  void CalculateUpdateDurationUptime();

  // Interface object with which we read/write persisted state. This must
  // be set by calling the Initialize method before calling any other method.
  PrefsInterface* prefs_;

  // This is the current response object from Omaha.
  OmahaResponse response_;

  // This stores a "signature" of the current response. The signature here
  // refers to a subset of the current response from Omaha.  Each update to
  // this value is persisted so we resume from the same value in case of a
  // process restart.
  std::string response_signature_;

  // The number of times we've tried to download the payload in full. This is
  // incremented each time we download the payload in full successsfully or
  // when we exhaust all failure limits for all URLs and are about to wrap
  // around back to the first URL.  Each update to this value is persisted so
  // we resume from the same value in case of a process restart.
  uint32_t payload_attempt_number_;

  // The index of the current URL.  This type is different from the one in the
  // accessor methods because PrefsInterface supports only int64_t but we want
  // to provide a stronger abstraction of uint32_t.  Each update to this value
  // is persisted so we resume from the same value in case of a process
  // restart.
  int64_t url_index_;

  // The count of failures encountered in the current attempt to download using
  // the current URL (specified by url_index_).  Each update to this value is
  // persisted so we resume from the same value in case of a process restart.
  int64_t url_failure_count_;

  // The timestamp until which we've to wait before attempting to download the
  // payload again, so as to backoff repeated downloads.
  base::Time backoff_expiry_time_;

  // The most recently calculated value of the update duration.
  base::TimeDelta update_duration_current_;

  // The point in time (wall-clock) that the update was started.
  base::Time update_timestamp_start_;

  // The point in time (wall-clock) that the update ended. If the update
  // is still in progress, this is set to the Epoch (e.g. 0).
  base::Time update_timestamp_end_;

  // The update duration uptime
  base::TimeDelta update_duration_uptime_;

  // The monotonic time when |update_duration_uptime_| was last set
  base::Time update_duration_uptime_timestamp_;

  // Returns the number of URLs in the current response.
  // Note: This value will be 0 if this method is called before we receive
  // the first valid Omaha response in this process.
  uint32_t GetNumUrls() {
    return response_.payload_urls.size();
  }

  // A small timespan used when comparing wall-clock times for coping
  // with the fact that clocks drift and consequently are adjusted
  // (either forwards or backwards) via NTP.
  static const base::TimeDelta kDurationSlack;

  DISALLOW_COPY_AND_ASSIGN(PayloadState);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_H__
