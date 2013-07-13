// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_CONSTANTS_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_CONSTANTS_H

namespace chromeos_update_engine {

// The name of the marker file used to trigger powerwash when post-install
// completes successfully so that the device is powerwashed on next reboot.
extern const char kPowerwashMarkerFile[];

// Path to the marker file we use to indicate we've recorded a system reboot.
extern const char kSystemRebootedMarkerFile[];

// The contents of the powerwash marker file.
extern const char kPowerwashCommand[];

// Directory for AU prefs that are preserved across powerwash.
extern const char kPowerwashSafePrefsDir[];

// The location where we store the AU preferences (state etc).
extern const char kPrefsDirectory[];

// Path to the stateful partition on the root filesystem.
extern const char kStatefulPartition[];

// Constants related to preferences.
extern const char kPrefsBackoffExpiryTime[];
extern const char kPrefsCertificateReportToSendDownload[];
extern const char kPrefsCertificateReportToSendUpdate[];
extern const char kPrefsCurrentBytesDownloaded[];
extern const char kPrefsCurrentResponseSignature[];
extern const char kPrefsCurrentUrlFailureCount[];
extern const char kPrefsCurrentUrlIndex[];
extern const char kPrefsDeltaUpdateFailures[];
extern const char kPrefsFullPayloadAttemptNumber[];
extern const char kPrefsLastActivePingDay[];
extern const char kPrefsLastRollCallPingDay[];
extern const char kPrefsManifestMetadataSize[];
extern const char kPrefsNumReboots[];
extern const char kPrefsNumResponsesSeen[];
extern const char kPrefsPayloadAttemptNumber[];
extern const char kPrefsPreviousVersion[];
extern const char kPrefsResumedUpdateFailures[];
extern const char kPrefsRollbackVersion[];
extern const char kPrefsSystemUpdatedMarker[];
extern const char kPrefsTargetVersionAttempt[];
extern const char kPrefsTargetVersionInstalledFrom[];
extern const char kPrefsTargetVersionUniqueId[];
extern const char kPrefsTotalBytesDownloaded[];
extern const char kPrefsUpdateCheckCount[];
extern const char kPrefsUpdateCheckResponseHash[];
extern const char kPrefsUpdateDurationUptime[];
extern const char kPrefsUpdateFirstSeenAt[];
extern const char kPrefsUpdateOverCellularPermission[];
extern const char kPrefsUpdateServerCertificate[];
extern const char kPrefsUpdateStateNextDataOffset[];
extern const char kPrefsUpdateStateNextOperation[];
extern const char kPrefsUpdateStateSHA256Context[];
extern const char kPrefsUpdateStateSignatureBlob[];
extern const char kPrefsUpdateStateSignedSHA256Context[];
extern const char kPrefsUpdateTimestampStart[];
extern const char kPrefsUrlSwitchCount[];
extern const char kPrefsWallClockWaitPeriod[];

// A download source is any combination of protocol and server (that's of
// interest to us when looking at UMA metrics) using which we may download
// the payload.
typedef enum {
  kDownloadSourceHttpsServer, // UMA Binary representation: 0001
  kDownloadSourceHttpServer,  // UMA Binary representation: 0010

  // Note: Add new sources only above this line.
  kNumDownloadSources
} DownloadSource;

// A payload can be a Full or Delta payload. In some cases, a Full payload is
// used even when a Delta payload was available for the update, called here
// ForcedFull. The PayloadType enum is only used to send UMA metrics about the
// successfully applied payload.
typedef enum {
  kPayloadTypeFull,
  kPayloadTypeDelta,
  kPayloadTypeForcedFull,

  // Note: Add new payload types only above this line.
  kNumPayloadTypes
} PayloadType;

// The default number of UMA buckets for metrics.
const int kNumDefaultUmaBuckets = 50;

// General constants
const int kNumBytesInOneMiB = 1024 * 1024;

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_CONSTANTS_H
