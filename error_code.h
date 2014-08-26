// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_ERROR_CODE_H_
#define UPDATE_ENGINE_ERROR_CODE_H_

#include <ostream>  // NOLINT(readability/streams)

namespace chromeos_update_engine {

// Action exit codes.
enum class ErrorCode : int {
  kSuccess = 0,
  kError = 1,
  kOmahaRequestError = 2,
  kOmahaResponseHandlerError = 3,
  kFilesystemCopierError = 4,
  kPostinstallRunnerError = 5,
  kPayloadMismatchedType = 6,
  kInstallDeviceOpenError = 7,
  kKernelDeviceOpenError = 8,
  kDownloadTransferError = 9,
  kPayloadHashMismatchError = 10,
  kPayloadSizeMismatchError = 11,
  kDownloadPayloadVerificationError = 12,
  kDownloadNewPartitionInfoError = 13,
  kDownloadWriteError = 14,
  kNewRootfsVerificationError = 15,
  kNewKernelVerificationError = 16,
  kSignedDeltaPayloadExpectedError = 17,
  kDownloadPayloadPubKeyVerificationError = 18,
  kPostinstallBootedFromFirmwareB = 19,
  kDownloadStateInitializationError = 20,
  kDownloadInvalidMetadataMagicString = 21,
  kDownloadSignatureMissingInManifest = 22,
  kDownloadManifestParseError = 23,
  kDownloadMetadataSignatureError = 24,
  kDownloadMetadataSignatureVerificationError = 25,
  kDownloadMetadataSignatureMismatch = 26,
  kDownloadOperationHashVerificationError = 27,
  kDownloadOperationExecutionError = 28,
  kDownloadOperationHashMismatch = 29,
  kOmahaRequestEmptyResponseError = 30,
  kOmahaRequestXMLParseError = 31,
  kDownloadInvalidMetadataSize = 32,
  kDownloadInvalidMetadataSignature = 33,
  kOmahaResponseInvalid = 34,
  kOmahaUpdateIgnoredPerPolicy = 35,
  kOmahaUpdateDeferredPerPolicy = 36,
  kOmahaErrorInHTTPResponse = 37,
  kDownloadOperationHashMissingError = 38,
  kDownloadMetadataSignatureMissingError = 39,
  kOmahaUpdateDeferredForBackoff = 40,
  kPostinstallPowerwashError = 41,
  kUpdateCanceledByChannelChange = 42,
  kPostinstallFirmwareRONotUpdatable = 43,
  kUnsupportedMajorPayloadVersion = 44,
  kUnsupportedMinorPayloadVersion = 45,
  kOmahaRequestXMLHasEntityDecl = 46,

  // VERY IMPORTANT! When adding new error codes:
  //
  // 1) Update tools/metrics/histograms/histograms.xml in Chrome.
  //
  // 2) Update the assorted switch statements in update_engine which won't
  //    build until this case is added.

  // Any code above this is sent to both Omaha and UMA as-is, except
  // kOmahaErrorInHTTPResponse (see error code 2000 for more details).
  // Codes/flags below this line is sent only to Omaha and not to UMA.

  // kUmaReportedMax is not an error code per se, it's just the count
  // of the number of enums above.  Add any new errors above this line if you
  // want them to show up on UMA. Stuff below this line will not be sent to UMA
  // but is used for other errors that are sent to Omaha. We don't assign any
  // particular value for this enum so that it's just one more than the last
  // one above and thus always represents the correct count of UMA metrics
  // buckets, even when new enums are added above this line in future. See
  // utils::SendErrorCodeToUma on how this enum is used.
  kUmaReportedMax,

  // use the 2xxx range to encode HTTP errors. These errors are available in
  // Dremel with the individual granularity. But for UMA purposes, all these
  // errors are aggregated into one: kOmahaErrorInHTTPResponse.
  kOmahaRequestHTTPResponseBase = 2000,  // + HTTP response code

  // TODO(jaysri): Move out all the bit masks into separate constants
  // outside the enum as part of fixing bug 34369.
  // Bit flags. Remember to update the mask below for new bits.

  // Set if boot mode not normal.
  // TODO(garnold) This is very debatable value to use, knowing that the
  // underlying type is a signed int (often, 32-bit). However, at this point
  // there are parts of the ecosystem that expect this to be a negative value,
  // so we preserve this semantics. This should be reconsidered if/when we
  // modify the implementation of ErrorCode into a properly encapsulated class.
  kDevModeFlag = 1 << 31,

  // Set if resuming an interruped update.
  kResumedFlag = 1 << 30,

  // Set if using a dev/test image as opposed to an MP-signed image.
  kTestImageFlag = 1 << 29,

  // Set if using devserver or Omaha sandbox (using crosh autest).
  kTestOmahaUrlFlag = 1 << 28,

  // Mask that indicates bit positions that are used to indicate special flags
  // that are embedded in the error code to provide additional context about
  // the system in which the error was encountered.
  kSpecialFlags = (kDevModeFlag | kResumedFlag | kTestImageFlag |
                   kTestOmahaUrlFlag)
};

inline std::ostream& operator<<(std::ostream& os, ErrorCode val) {
  return os << static_cast<int>(val);
}

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_ERROR_CODE_H_
