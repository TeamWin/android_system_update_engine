// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_ERROR_CODE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_ERROR_CODE_H__

namespace chromeos_update_engine {

// Action exit codes.
enum ErrorCode {
  kErrorCodeSuccess = 0,
  kErrorCodeError = 1,
  kErrorCodeOmahaRequestError = 2,
  kErrorCodeOmahaResponseHandlerError = 3,
  kErrorCodeFilesystemCopierError = 4,
  kErrorCodePostinstallRunnerError = 5,
  kErrorCodePayloadMismatchedType = 6,
  kErrorCodeInstallDeviceOpenError = 7,
  kErrorCodeKernelDeviceOpenError = 8,
  kErrorCodeDownloadTransferError = 9,
  kErrorCodePayloadHashMismatchError = 10,
  kErrorCodePayloadSizeMismatchError = 11,
  kErrorCodeDownloadPayloadVerificationError = 12,
  kErrorCodeDownloadNewPartitionInfoError = 13,
  kErrorCodeDownloadWriteError = 14,
  kErrorCodeNewRootfsVerificationError = 15,
  kErrorCodeNewKernelVerificationError = 16,
  kErrorCodeSignedDeltaPayloadExpectedError = 17,
  kErrorCodeDownloadPayloadPubKeyVerificationError = 18,
  kErrorCodePostinstallBootedFromFirmwareB = 19,
  kErrorCodeDownloadStateInitializationError = 20,
  kErrorCodeDownloadInvalidMetadataMagicString = 21,
  kErrorCodeDownloadSignatureMissingInManifest = 22,
  kErrorCodeDownloadManifestParseError = 23,
  kErrorCodeDownloadMetadataSignatureError = 24,
  kErrorCodeDownloadMetadataSignatureVerificationError = 25,
  kErrorCodeDownloadMetadataSignatureMismatch = 26,
  kErrorCodeDownloadOperationHashVerificationError = 27,
  kErrorCodeDownloadOperationExecutionError = 28,
  kErrorCodeDownloadOperationHashMismatch = 29,
  kErrorCodeOmahaRequestEmptyResponseError = 30,
  kErrorCodeOmahaRequestXMLParseError = 31,
  kErrorCodeDownloadInvalidMetadataSize = 32,
  kErrorCodeDownloadInvalidMetadataSignature = 33,
  kErrorCodeOmahaResponseInvalid = 34,
  kErrorCodeOmahaUpdateIgnoredPerPolicy = 35,
  kErrorCodeOmahaUpdateDeferredPerPolicy = 36,
  kErrorCodeOmahaErrorInHTTPResponse = 37,
  kErrorCodeDownloadOperationHashMissingError = 38,
  kErrorCodeDownloadMetadataSignatureMissingError = 39,
  kErrorCodeOmahaUpdateDeferredForBackoff = 40,
  kErrorCodePostinstallPowerwashError = 41,
  kErrorCodeUpdateCanceledByChannelChange = 42,
  kErrorCodePostinstallFirmwareRONotUpdatable = 43,
  kErrorCodeUnsupportedMajorPayloadVersion = 44,
  kErrorCodeUnsupportedMinorPayloadVersion = 45,

  // VERY IMPORTANT! When adding new error codes:
  //
  // 1) Update tools/metrics/histograms/histograms.xml in Chrome.
  //
  // 2) Update the assorted switch statements in update_engine which won't
  //    build until this case is added.

  // Any code above this is sent to both Omaha and UMA as-is, except
  // kErrorCodeOmahaErrorInHTTPResponse (see error code 2000 for more details).
  // Codes/flags below this line is sent only to Omaha and not to UMA.

  // kErrorCodeUmaReportedMax is not an error code per se, it's just the count
  // of the number of enums above.  Add any new errors above this line if you
  // want them to show up on UMA. Stuff below this line will not be sent to UMA
  // but is used for other errors that are sent to Omaha. We don't assign any
  // particular value for this enum so that it's just one more than the last
  // one above and thus always represents the correct count of UMA metrics
  // buckets, even when new enums are added above this line in future. See
  // utils::SendErrorCodeToUma on how this enum is used.
  kErrorCodeUmaReportedMax,

  // use the 2xxx range to encode HTTP errors. These errors are available in
  // Dremel with the individual granularity. But for UMA purposes, all these
  // errors are aggregated into one: kErrorCodeOmahaErrorInHTTPResponse.
  kErrorCodeOmahaRequestHTTPResponseBase = 2000,  // + HTTP response code

  // TODO(jaysri): Move out all the bit masks into separate constants
  // outside the enum as part of fixing bug 34369.
  // Bit flags. Remember to update the mask below for new bits.

  // Set if boot mode not normal.
  kErrorCodeDevModeFlag        = 1 << 31,

  // Set if resuming an interruped update.
  kErrorCodeResumedFlag         = 1 << 30,

  // Set if using a dev/test image as opposed to an MP-signed image.
  kErrorCodeTestImageFlag       = 1 << 29,

  // Set if using devserver or Omaha sandbox (using crosh autest).
  kErrorCodeTestOmahaUrlFlag    = 1 << 28,

  // Mask that indicates bit positions that are used to indicate special flags
  // that are embedded in the error code to provide additional context about
  // the system in which the error was encountered.
  kErrorCodeSpecialFlags = (kErrorCodeDevModeFlag |
                            kErrorCodeResumedFlag |
                            kErrorCodeTestImageFlag |
                            kErrorCodeTestOmahaUrlFlag)
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_PROCESSOR_H__
