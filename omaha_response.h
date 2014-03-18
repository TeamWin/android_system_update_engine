// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

namespace chromeos_update_engine {

// This struct encapsulates the data Omaha's response for the request.
// The strings in this struct are not XML escaped.
struct OmahaResponse {
  OmahaResponse()
      : update_exists(false),
        poll_interval(0),
        size(0),
        metadata_size(0),
        max_days_to_scatter(0),
        max_failure_count_per_url(0),
        prompt(false),
        is_delta_payload(false),
        disable_payload_backoff(false),
        disable_p2p_for_downloading(false),
        disable_p2p_for_sharing(false),
        install_date_days(-1) {}

  // True iff there is an update to be downloaded.
  bool update_exists;

  // If non-zero, server-dictated poll interval in seconds.
  int poll_interval;

  // These are only valid if update_exists is true:
  std::string version;

  // The ordered list of URLs in the Omaha response. Each item is a complete
  // URL (i.e. in terms of Omaha XML, each value is a urlBase + packageName)
  std::vector<std::string> payload_urls;

  std::string more_info_url;
  std::string hash;
  std::string metadata_signature;
  std::string deadline;
  off_t size;
  off_t metadata_size;
  int max_days_to_scatter;
  // The number of URL-related failures to tolerate before moving on to the
  // next URL in the current pass. This is a configurable value from the
  // Omaha Response attribute, if ever we need to fine tune the behavior.
  uint32_t max_failure_count_per_url;
  bool prompt;

  // True if the payload described in this response is a delta payload.
  // False if it's a full payload.
  bool is_delta_payload;

  // True if the Omaha rule instructs us to disable the backoff logic
  // on the client altogether. False otherwise.
  bool disable_payload_backoff;

  // True if the Omaha rule instructs us to disable p2p for downloading.
  bool disable_p2p_for_downloading;

  // True if the Omaha rule instructs us to disable p2p for sharing.
  bool disable_p2p_for_sharing;

  // If not blank, a base-64 encoded representation of the PEM-encoded
  // public key in the response.
  std::string public_key_rsa;

  // If not -1, the number of days since the epoch Jan 1, 2007 0:00
  // PST, according to the Omaha Server's clock and timezone (PST8PDT,
  // aka "Pacific Time".)
  int install_date_days;
};
COMPILE_ASSERT(sizeof(off_t) == 8, off_t_not_64bit);

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_H_
