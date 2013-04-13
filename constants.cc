// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/constants.h"

namespace chromeos_update_engine {

const char kPowerwashMarkerFile[] =
  "/mnt/stateful_partition/factory_install_reset";

const char kPowerwashCommand[] = "safe fast\n";

// Constants defining keys for the persisted state of update engine.
const char kPrefsBackoffExpiryTime[] = "backoff-expiry-time";
const char kPrefsCertificateReportToSendDownload[] =
    "certificate-report-to-send-download";
const char kPrefsCertificateReportToSendUpdate[] =
    "certificate-report-to-send-update";
const char kPrefsCurrentBytesDownloaded[] = "current-bytes-downloaded";
const char kPrefsCurrentResponseSignature[] = "current-response-signature";
const char kPrefsCurrentUrlFailureCount[] = "current-url-failure-count";
const char kPrefsCurrentUrlIndex[] = "current-url-index";
const char kPrefsDeltaUpdateFailures[] = "delta-update-failures";
const char kPrefsLastActivePingDay[] = "last-active-ping-day";
const char kPrefsLastRollCallPingDay[] = "last-roll-call-ping-day";
const char kPrefsManifestMetadataSize[] = "manifest-metadata-size";
const char kPrefsPayloadAttemptNumber[] = "payload-attempt-number";
const char kPrefsPreviousVersion[] = "previous-version";
const char kPrefsResumedUpdateFailures[] = "resumed-update-failures";
const char kPrefsTotalBytesDownloaded[] = "total-bytes-downloaded";
const char kPrefsUpdateCheckCount[] = "update-check-count";
const char kPrefsUpdateCheckResponseHash[] = "update-check-response-hash";
const char kPrefsUpdateFirstSeenAt[] = "update-first-seen-at";
const char kPrefsUpdateServerCertificate[] = "update-server-cert";
const char kPrefsUpdateStateNextDataOffset[] = "update-state-next-data-offset";
const char kPrefsUpdateStateNextOperation[] = "update-state-next-operation";
const char kPrefsUpdateStateSHA256Context[] = "update-state-sha-256-context";
const char kPrefsUpdateStateSignatureBlob[] = "update-state-signature-blob";
const char kPrefsUpdateStateSignedSHA256Context[] =
    "update-state-signed-sha-256-context";
const char kPrefsWallClockWaitPeriod[] = "wall-clock-wait-period";
const char kPrefsUpdateTimestampStart[] = "update-timestamp-start";
const char kPrefsUpdateDurationUptime[] = "update-duration-uptime";

}
