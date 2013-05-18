// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_INTERFACE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_INTERFACE_H__

#include <string>

#include "update_engine/constants.h"
#include "update_engine/action_processor.h"
#include "update_engine/omaha_response.h"

namespace chromeos_update_engine {

// Describes the methods that need to be implemented by the PayloadState class.
// This interface has been carved out to support mocking of the PayloadState
// object.
class PayloadStateInterface {
 public:
  // Sets the internal payload state based on the given Omaha response. This
  // response could be the same or different from the one for which we've stored
  // the internal state. If it's different, then this method resets all the
  // internal state corresponding to the old response. Since the Omaha response
  // has a lot of fields that are not related to payload state, it uses only
  // a subset of the fields in the Omaha response to compare equality.
  virtual void SetResponse(const OmahaResponse& response) = 0;

  // This method should be called whenever we have completed downloading all
  // the bytes of a payload and have verified that its size and hash match the
  // expected values. We use this notificaiton to increment the payload attempt
  // number so that the throttle the next attempt to download the same payload
  // (in case there's an error in subsequent steps such as post-install)
  // appropriately.
  virtual void DownloadComplete() = 0;

  // This method should be called whenever we receive new bytes from the
  // network for the current payload. We use this notification to reset the
  // failure count for a given URL since receipt of some bytes means we are
  // able to make forward progress with the current URL.
  virtual void DownloadProgress(size_t count) = 0;

  // This method should be called every time we resume an update attempt.
  virtual void UpdateResumed() = 0;

  // This method should be called every time we begin a new update. This method
  // should not be called when we resume an update from the previously
  // downloaded point. This is used to reset the metrics for each new update.
  virtual void UpdateRestarted() = 0;

  // This method should be called once after an update attempt succeeds. This
  // is when the relevant UMA metrics that are tracked on a per-update-basis
  // are uploaded to the UMA server.
  virtual void UpdateSucceeded() = 0;

  // This method should be called whenever an update attempt fails with the
  // given error code. We use this notification to update the payload state
  // depending on the type of the error that happened.
  virtual void UpdateFailed(ErrorCode error) = 0;

  // Returns true if we should backoff the current download attempt.
  // False otherwise.
  virtual bool ShouldBackoffDownload() = 0;

  // Returns the currently stored response "signature". The signature  is a
  // subset of fields that are of interest to the PayloadState behavior.
  virtual std::string GetResponseSignature() = 0;

  // Returns the payload attempt number.
  virtual uint32_t GetPayloadAttemptNumber() = 0;

  // Returns the current URL. Returns an empty string if there's no valid URL.
  virtual std::string GetCurrentUrl() = 0;

  // Returns the current URL's failure count.
  virtual uint32_t GetUrlFailureCount() = 0;

  // Returns the total number of times a new URL has been switched to
  // for the current response.
  virtual uint32_t GetUrlSwitchCount() = 0;

  // Returns the expiry time for the current backoff period.
  virtual base::Time GetBackoffExpiryTime() = 0;

  // Returns the elapsed time used for this update, including time
  // where the device is powered off and sleeping. If the
  // update has not completed, returns the time spent so far.
  virtual base::TimeDelta GetUpdateDuration() = 0;

  // Returns the time used for this update not including time when
  // the device is powered off or sleeping. If the update has not
  // completed, returns the time spent so far.
  virtual base::TimeDelta GetUpdateDurationUptime() = 0;

  // Returns the number of bytes that have been downloaded for each source for
  // each new update attempt. If we resume an update, we'll continue from the
  // previous value, but if we get a new response or if the previous attempt
  // failed, we'll reset this to 0 to start afresh.
  virtual uint64_t GetCurrentBytesDownloaded(DownloadSource source) = 0;

  // Returns the total number of bytes that have been downloaded for each
  // source since the the last successful update. This is used to compute the
  // overhead we incur.
  virtual uint64_t GetTotalBytesDownloaded(DownloadSource source) = 0;

  // Returns the reboot count for this update attempt.
  virtual uint32_t GetNumReboots() = 0;
 };

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_INTERFACE_H__
