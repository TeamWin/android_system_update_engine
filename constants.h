// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_CONSTANTS_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_CONSTANTS_H

namespace chromeos_update_engine {

// The name of the marker file used to trigger powerwash when post-install
// completes successfully so that the device is powerwashed on next reboot.
extern const char kPowerwashMarkerFile[];

// The contents of the powerwash marker file.
extern const char kPowerwashCommand[];

// Constants related to preferences.
extern const char kPrefsBackoffExpiryTime[];
extern const char kPrefsCertificateReportToSendDownload[];
extern const char kPrefsCertificateReportToSendUpdate[];
extern const char kPrefsCurrentBytesDownloaded[];
extern const char kPrefsCurrentResponseSignature[];
extern const char kPrefsCurrentUrlFailureCount[];
extern const char kPrefsCurrentUrlIndex[];
extern const char kPrefsDeltaUpdateFailures[];
extern const char kPrefsLastActivePingDay[];
extern const char kPrefsLastRollCallPingDay[];
extern const char kPrefsManifestMetadataSize[];
extern const char kPrefsPayloadAttemptNumber[];
extern const char kPrefsPreviousVersion[];
extern const char kPrefsResumedUpdateFailures[];
extern const char kPrefsTotalBytesDownloaded[];
extern const char kPrefsUpdateCheckCount[];
extern const char kPrefsUpdateCheckResponseHash[];
extern const char kPrefsUpdateFirstSeenAt[];
extern const char kPrefsUpdateServerCertificate[];
extern const char kPrefsUpdateStateNextDataOffset[];
extern const char kPrefsUpdateStateNextOperation[];
extern const char kPrefsUpdateStateSHA256Context[];
extern const char kPrefsUpdateStateSignatureBlob[];
extern const char kPrefsUpdateStateSignedSHA256Context[];
extern const char kPrefsWallClockWaitPeriod[];
extern const char kPrefsUpdateTimestampStart[];
extern const char kPrefsUpdateDurationUptime[];

// A download source is any combination of protocol and server (that's of
// interest to us when looking at UMA metrics) using which we may download
// the payload.
typedef enum {
  kDownloadSourceHttpsServer, // UMA Binary representation: 0001
  kDownloadSourceHttpServer,  // UMA Binary representation: 0010

  // Note: Add new sources only above this line.
  kNumDownloadSources
} DownloadSource;

// The default number of UMA buckets for metrics.
const int kNumDefaultUmaBuckets = 50;

// General constants
const int kNumBytesInOneMiB = 1024 * 1024;

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_CONSTANTS_H
