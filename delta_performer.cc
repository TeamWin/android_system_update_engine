// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_performer.h"

#include <endian.h>
#include <errno.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/format_macros.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/data_encoding.h>
#include <google/protobuf/repeated_field.h>

#include "update_engine/bzip_extent_writer.h"
#include "update_engine/constants.h"
#include "update_engine/extent_writer.h"
#include "update_engine/hardware_interface.h"
#if USE_MTD
#include "update_engine/mtd_file_descriptor.h"
#endif
#include "update_engine/payload_constants.h"
#include "update_engine/payload_state_interface.h"
#include "update_engine/payload_verifier.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/subprocess.h"
#include "update_engine/terminator.h"
#include "update_engine/update_attempter.h"

using google::protobuf::RepeatedPtrField;
using std::min;
using std::string;
using std::unique_ptr;
using std::vector;

namespace chromeos_update_engine {

const uint64_t DeltaPerformer::kDeltaVersionSize = 8;
const uint64_t DeltaPerformer::kDeltaManifestSizeSize = 8;
const uint64_t DeltaPerformer::kSupportedMajorPayloadVersion = 1;
const uint64_t DeltaPerformer::kSupportedMinorPayloadVersion = 2;
const uint64_t DeltaPerformer::kFullPayloadMinorVersion = 0;

const char DeltaPerformer::kUpdatePayloadPublicKeyPath[] =
    "/usr/share/update_engine/update-payload-key.pub.pem";
const unsigned DeltaPerformer::kProgressLogMaxChunks = 10;
const unsigned DeltaPerformer::kProgressLogTimeoutSeconds = 30;
const unsigned DeltaPerformer::kProgressDownloadWeight = 50;
const unsigned DeltaPerformer::kProgressOperationsWeight = 50;

const uint32_t kInPlaceMinorPayloadVersion = 1;
const uint32_t kSourceMinorPayloadVersion = 2;

namespace {
const int kUpdateStateOperationInvalid = -1;
const int kMaxResumedUpdateFailures = 10;
#if USE_MTD
const int kUbiVolumeAttachTimeout = 5 * 60;
#endif

FileDescriptorPtr CreateFileDescriptor(const char* path) {
  FileDescriptorPtr ret;
#if USE_MTD
  if (strstr(path, "/dev/ubi") == path) {
    if (!UbiFileDescriptor::IsUbi(path)) {
      // The volume might not have been attached at boot time.
      int volume_no;
      if (utils::SplitPartitionName(path, nullptr, &volume_no)) {
        utils::TryAttachingUbiVolume(volume_no, kUbiVolumeAttachTimeout);
      }
    }
    if (UbiFileDescriptor::IsUbi(path)) {
      LOG(INFO) << path << " is a UBI device.";
      ret.reset(new UbiFileDescriptor);
    }
  } else if (MtdFileDescriptor::IsMtd(path)) {
    LOG(INFO) << path << " is an MTD device.";
    ret.reset(new MtdFileDescriptor);
  } else {
    LOG(INFO) << path << " is not an MTD nor a UBI device.";
#endif
    ret.reset(new EintrSafeFileDescriptor);
#if USE_MTD
  }
#endif
  return ret;
}

// Opens path for read/write. On success returns an open FileDescriptor
// and sets *err to 0. On failure, sets *err to errno and returns nullptr.
FileDescriptorPtr OpenFile(const char* path, int* err) {
  FileDescriptorPtr fd = CreateFileDescriptor(path);
  int mode = O_RDWR;
#if USE_MTD
  // On NAND devices, we can either read, or write, but not both. So here we
  // use O_WRONLY.
  if (UbiFileDescriptor::IsUbi(path) || MtdFileDescriptor::IsMtd(path)) {
    mode = O_WRONLY;
  }
#endif
  if (!fd->Open(path, mode, 000)) {
    *err = errno;
    PLOG(ERROR) << "Unable to open file " << path;
    return nullptr;
  }
  *err = 0;
  return fd;
}
}  // namespace


// Computes the ratio of |part| and |total|, scaled to |norm|, using integer
// arithmetic.
static uint64_t IntRatio(uint64_t part, uint64_t total, uint64_t norm) {
  return part * norm / total;
}

void DeltaPerformer::LogProgress(const char* message_prefix) {
  // Format operations total count and percentage.
  string total_operations_str("?");
  string completed_percentage_str("");
  if (num_total_operations_) {
    total_operations_str = base::StringPrintf("%zu", num_total_operations_);
    // Upcasting to 64-bit to avoid overflow, back to size_t for formatting.
    completed_percentage_str =
        base::StringPrintf(" (%" PRIu64 "%%)",
                     IntRatio(next_operation_num_, num_total_operations_,
                              100));
  }

  // Format download total count and percentage.
  size_t payload_size = install_plan_->payload_size;
  string payload_size_str("?");
  string downloaded_percentage_str("");
  if (payload_size) {
    payload_size_str = base::StringPrintf("%zu", payload_size);
    // Upcasting to 64-bit to avoid overflow, back to size_t for formatting.
    downloaded_percentage_str =
        base::StringPrintf(" (%" PRIu64 "%%)",
                     IntRatio(total_bytes_received_, payload_size, 100));
  }

  LOG(INFO) << (message_prefix ? message_prefix : "") << next_operation_num_
            << "/" << total_operations_str << " operations"
            << completed_percentage_str << ", " << total_bytes_received_
            << "/" << payload_size_str << " bytes downloaded"
            << downloaded_percentage_str << ", overall progress "
            << overall_progress_ << "%";
}

void DeltaPerformer::UpdateOverallProgress(bool force_log,
                                           const char* message_prefix) {
  // Compute our download and overall progress.
  unsigned new_overall_progress = 0;
  COMPILE_ASSERT(kProgressDownloadWeight + kProgressOperationsWeight == 100,
                 progress_weight_dont_add_up);
  // Only consider download progress if its total size is known; otherwise
  // adjust the operations weight to compensate for the absence of download
  // progress. Also, make sure to cap the download portion at
  // kProgressDownloadWeight, in case we end up downloading more than we
  // initially expected (this indicates a problem, but could generally happen).
  // TODO(garnold) the correction of operations weight when we do not have the
  // total payload size, as well as the conditional guard below, should both be
  // eliminated once we ensure that the payload_size in the install plan is
  // always given and is non-zero. This currently isn't the case during unit
  // tests (see chromium-os:37969).
  size_t payload_size = install_plan_->payload_size;
  unsigned actual_operations_weight = kProgressOperationsWeight;
  if (payload_size)
    new_overall_progress += min(
        static_cast<unsigned>(IntRatio(total_bytes_received_, payload_size,
                                       kProgressDownloadWeight)),
        kProgressDownloadWeight);
  else
    actual_operations_weight += kProgressDownloadWeight;

  // Only add completed operations if their total number is known; we definitely
  // expect an update to have at least one operation, so the expectation is that
  // this will eventually reach |actual_operations_weight|.
  if (num_total_operations_)
    new_overall_progress += IntRatio(next_operation_num_, num_total_operations_,
                                     actual_operations_weight);

  // Progress ratio cannot recede, unless our assumptions about the total
  // payload size, total number of operations, or the monotonicity of progress
  // is breached.
  if (new_overall_progress < overall_progress_) {
    LOG(WARNING) << "progress counter receded from " << overall_progress_
                 << "% down to " << new_overall_progress << "%; this is a bug";
    force_log = true;
  }
  overall_progress_ = new_overall_progress;

  // Update chunk index, log as needed: if forced by called, or we completed a
  // progress chunk, or a timeout has expired.
  base::Time curr_time = base::Time::Now();
  unsigned curr_progress_chunk =
      overall_progress_ * kProgressLogMaxChunks / 100;
  if (force_log || curr_progress_chunk > last_progress_chunk_ ||
      curr_time > forced_progress_log_time_) {
    forced_progress_log_time_ = curr_time + forced_progress_log_wait_;
    LogProgress(message_prefix);
  }
  last_progress_chunk_ = curr_progress_chunk;
}


size_t DeltaPerformer::CopyDataToBuffer(const char** bytes_p, size_t* count_p,
                                        size_t max) {
  const size_t count = *count_p;
  if (!count)
    return 0;  // Special case shortcut.
  size_t read_len = min(count, max - buffer_.size());
  const char* bytes_start = *bytes_p;
  const char* bytes_end = bytes_start + read_len;
  buffer_.insert(buffer_.end(), bytes_start, bytes_end);
  *bytes_p = bytes_end;
  *count_p = count - read_len;
  return read_len;
}


bool DeltaPerformer::HandleOpResult(bool op_result, const char* op_type_name,
                                    ErrorCode* error) {
  if (op_result)
    return true;

  LOG(ERROR) << "Failed to perform " << op_type_name << " operation "
             << next_operation_num_;
  *error = ErrorCode::kDownloadOperationExecutionError;
  return false;
}

int DeltaPerformer::Open(const char* path, int flags, mode_t mode) {
  int err;
  fd_ = OpenFile(path, &err);
  if (fd_)
    path_ = path;
  return -err;
}

bool DeltaPerformer::OpenKernel(const char* kernel_path) {
  int err;
  kernel_fd_ = OpenFile(kernel_path, &err);
  if (kernel_fd_)
    kernel_path_ = kernel_path;
  return static_cast<bool>(kernel_fd_);
}

bool DeltaPerformer::OpenSourceRootfs(const string& source_path) {
  int err;
  source_fd_ = OpenFile(source_path.c_str(), &err);
  return static_cast<bool>(source_fd_);
}

bool DeltaPerformer::OpenSourceKernel(const string& source_kernel_path) {
  int err;
  source_kernel_fd_ = OpenFile(source_kernel_path.c_str(), &err);
  return static_cast<bool>(source_kernel_fd_);
}

int DeltaPerformer::Close() {
  int err = 0;
  if (!kernel_fd_->Close()) {
    err = errno;
    PLOG(ERROR) << "Unable to close kernel fd:";
  }
  if (!fd_->Close()) {
    err = errno;
    PLOG(ERROR) << "Unable to close rootfs fd:";
  }
  if (source_fd_ && !source_fd_->Close()) {
    err = errno;
    PLOG(ERROR) << "Unable to close source rootfs fd:";
  }
  if (source_kernel_fd_ && !source_kernel_fd_->Close()) {
    err = errno;
    PLOG(ERROR) << "Unable to close source kernel fd:";
  }
  LOG_IF(ERROR, !hash_calculator_.Finalize()) << "Unable to finalize the hash.";
  fd_.reset();  // Set to invalid so that calls to Open() will fail.
  kernel_fd_.reset();
  source_fd_.reset();
  source_kernel_fd_.reset();
  path_ = "";
  if (!buffer_.empty()) {
    LOG(INFO) << "Discarding " << buffer_.size() << " unused downloaded bytes";
    if (err >= 0)
      err = 1;
  }
  return -err;
}

namespace {

void LogPartitionInfoHash(const PartitionInfo& info, const string& tag) {
  string sha256 = chromeos::data_encoding::Base64Encode(info.hash());
  LOG(INFO) << "PartitionInfo " << tag << " sha256: " << sha256
            << " size: " << info.size();
}

void LogPartitionInfo(const DeltaArchiveManifest& manifest) {
  if (manifest.has_old_kernel_info())
    LogPartitionInfoHash(manifest.old_kernel_info(), "old_kernel_info");
  if (manifest.has_old_rootfs_info())
    LogPartitionInfoHash(manifest.old_rootfs_info(), "old_rootfs_info");
  if (manifest.has_new_kernel_info())
    LogPartitionInfoHash(manifest.new_kernel_info(), "new_kernel_info");
  if (manifest.has_new_rootfs_info())
    LogPartitionInfoHash(manifest.new_rootfs_info(), "new_rootfs_info");
}

}  // namespace

uint64_t DeltaPerformer::GetVersionOffset() {
  // Manifest size is stored right after the magic string and the version.
  return strlen(kDeltaMagic);
}

uint64_t DeltaPerformer::GetManifestSizeOffset() {
  // Manifest size is stored right after the magic string and the version.
  return strlen(kDeltaMagic) + kDeltaVersionSize;
}

uint64_t DeltaPerformer::GetManifestOffset() {
  // Actual manifest begins right after the manifest size field.
  return GetManifestSizeOffset() + kDeltaManifestSizeSize;
}

uint64_t DeltaPerformer::GetMetadataSize() const {
  return metadata_size_;
}

uint32_t DeltaPerformer::GetMinorVersion() const {
  if (manifest_.has_minor_version()) {
    return manifest_.minor_version();
  } else {
    return (install_plan_->is_full_update ?
            kFullPayloadMinorVersion :
            kSupportedMinorPayloadVersion);
  }
}

bool DeltaPerformer::GetManifest(DeltaArchiveManifest* out_manifest_p) const {
  if (!manifest_parsed_)
    return false;
  *out_manifest_p = manifest_;
  return true;
}


DeltaPerformer::MetadataParseResult DeltaPerformer::ParsePayloadMetadata(
    const chromeos::Blob& payload, ErrorCode* error) {
  *error = ErrorCode::kSuccess;
  const uint64_t manifest_offset = GetManifestOffset();
  uint64_t manifest_size = (metadata_size_ ?
                            metadata_size_ - manifest_offset : 0);

  if (!manifest_size) {
    // Ensure we have data to cover the payload header.
    if (payload.size() < manifest_offset)
      return kMetadataParseInsufficientData;

    // Validate the magic string.
    if (memcmp(payload.data(), kDeltaMagic, strlen(kDeltaMagic)) != 0) {
      LOG(ERROR) << "Bad payload format -- invalid delta magic.";
      *error = ErrorCode::kDownloadInvalidMetadataMagicString;
      return kMetadataParseError;
    }

    // Extract the payload version from the metadata.
    uint64_t major_payload_version;
    COMPILE_ASSERT(sizeof(major_payload_version) == kDeltaVersionSize,
                   major_payload_version_size_mismatch);
    memcpy(&major_payload_version,
           &payload[GetVersionOffset()],
           kDeltaVersionSize);
    // switch big endian to host
    major_payload_version = be64toh(major_payload_version);

    if (major_payload_version != kSupportedMajorPayloadVersion) {
      LOG(ERROR) << "Bad payload format -- unsupported payload version: "
          << major_payload_version;
      *error = ErrorCode::kUnsupportedMajorPayloadVersion;
      return kMetadataParseError;
    }

    // Next, parse the manifest size.
    COMPILE_ASSERT(sizeof(manifest_size) == kDeltaManifestSizeSize,
                   manifest_size_size_mismatch);
    memcpy(&manifest_size,
           &payload[GetManifestSizeOffset()],
           kDeltaManifestSizeSize);
    manifest_size = be64toh(manifest_size);  // switch big endian to host

    // If the metadata size is present in install plan, check for it immediately
    // even before waiting for that many number of bytes to be downloaded in the
    // payload. This will prevent any attack which relies on us downloading data
    // beyond the expected metadata size.
    metadata_size_ = manifest_offset + manifest_size;
    if (install_plan_->hash_checks_mandatory) {
      if (install_plan_->metadata_size != metadata_size_) {
        LOG(ERROR) << "Mandatory metadata size in Omaha response ("
                   << install_plan_->metadata_size
                   << ") is missing/incorrect, actual = " << metadata_size_;
        *error = ErrorCode::kDownloadInvalidMetadataSize;
        return kMetadataParseError;
      }
    }
  }

  // Now that we have validated the metadata size, we should wait for the full
  // metadata to be read in before we can parse it.
  if (payload.size() < metadata_size_)
    return kMetadataParseInsufficientData;

  // Log whether we validated the size or simply trusting what's in the payload
  // here. This is logged here (after we received the full metadata data) so
  // that we just log once (instead of logging n times) if it takes n
  // DeltaPerformer::Write calls to download the full manifest.
  if (install_plan_->metadata_size == metadata_size_) {
    LOG(INFO) << "Manifest size in payload matches expected value from Omaha";
  } else {
    // For mandatory-cases, we'd have already returned a kMetadataParseError
    // above. We'll be here only for non-mandatory cases. Just send a UMA stat.
    LOG(WARNING) << "Ignoring missing/incorrect metadata size ("
                 << install_plan_->metadata_size
                 << ") in Omaha response as validation is not mandatory. "
                 << "Trusting metadata size in payload = " << metadata_size_;
    SendUmaStat(ErrorCode::kDownloadInvalidMetadataSize);
  }

  // We have the full metadata in |payload|. Verify its integrity
  // and authenticity based on the information we have in Omaha response.
  *error = ValidateMetadataSignature(payload.data(), metadata_size_);
  if (*error != ErrorCode::kSuccess) {
    if (install_plan_->hash_checks_mandatory) {
      // The autoupdate_CatchBadSignatures test checks for this string
      // in log-files. Keep in sync.
      LOG(ERROR) << "Mandatory metadata signature validation failed";
      return kMetadataParseError;
    }

    // For non-mandatory cases, just send a UMA stat.
    LOG(WARNING) << "Ignoring metadata signature validation failures";
    SendUmaStat(*error);
    *error = ErrorCode::kSuccess;
  }

  // The payload metadata is deemed valid, it's safe to parse the protobuf.
  if (!manifest_.ParseFromArray(&payload[manifest_offset], manifest_size)) {
    LOG(ERROR) << "Unable to parse manifest in update file.";
    *error = ErrorCode::kDownloadManifestParseError;
    return kMetadataParseError;
  }

  manifest_parsed_ = true;
  return kMetadataParseSuccess;
}

// Wrapper around write. Returns true if all requested bytes
// were written, or false on any error, regardless of progress
// and stores an action exit code in |error|.
bool DeltaPerformer::Write(const void* bytes, size_t count, ErrorCode *error) {
  *error = ErrorCode::kSuccess;

  const char* c_bytes = reinterpret_cast<const char*>(bytes);
  system_state_->payload_state()->DownloadProgress(count);

  // Update the total byte downloaded count and the progress logs.
  total_bytes_received_ += count;
  UpdateOverallProgress(false, "Completed ");

  while (!manifest_valid_) {
    // Read data up to the needed limit; this is either the payload header size,
    // or the full metadata size (once it becomes known).
    const bool do_read_header = !metadata_size_;
    CopyDataToBuffer(&c_bytes, &count,
                     (do_read_header ? GetManifestOffset() :
                      metadata_size_));

    MetadataParseResult result = ParsePayloadMetadata(buffer_, error);
    if (result == kMetadataParseError)
      return false;
    if (result == kMetadataParseInsufficientData) {
      // If we just processed the header, make an attempt on the manifest.
      if (do_read_header && metadata_size_)
        continue;

      return true;
    }

    // Checks the integrity of the payload manifest.
    if ((*error = ValidateManifest()) != ErrorCode::kSuccess)
      return false;
    manifest_valid_ = true;

    // Clear the download buffer.
    DiscardBuffer(false);
    LOG_IF(WARNING, !prefs_->SetInt64(kPrefsManifestMetadataSize,
                                      metadata_size_))
        << "Unable to save the manifest metadata size.";

    LogPartitionInfo(manifest_);
    if (!PrimeUpdateState()) {
      *error = ErrorCode::kDownloadStateInitializationError;
      LOG(ERROR) << "Unable to prime the update state.";
      return false;
    }

    // Open source fds if we have a delta payload with minor version 2.
    if (!install_plan_->is_full_update &&
        GetMinorVersion() == kSourceMinorPayloadVersion) {
      if (!OpenSourceRootfs(install_plan_->source_path)) {
        LOG(ERROR) << "Unable to open source rootfs partition file "
                   << install_plan_->source_path;
        Close();
        return false;
      }
      if (!OpenSourceKernel(install_plan_->kernel_source_path)) {
        LOG(ERROR) << "Unable to open source kernel partition file "
                   << install_plan_->kernel_source_path;
        Close();
        return false;
      }
    }

    num_rootfs_operations_ = manifest_.install_operations_size();
    num_total_operations_ =
        num_rootfs_operations_ + manifest_.kernel_install_operations_size();
    if (next_operation_num_ > 0)
      UpdateOverallProgress(true, "Resuming after ");
    LOG(INFO) << "Starting to apply update payload operations";
  }

  while (next_operation_num_ < num_total_operations_) {
    // Check if we should cancel the current attempt for any reason.
    // In this case, *error will have already been populated with the reason
    // why we're cancelling.
    if (system_state_->update_attempter()->ShouldCancel(error))
      return false;

    const bool is_kernel_partition =
        (next_operation_num_ >= num_rootfs_operations_);
    const DeltaArchiveManifest_InstallOperation &op =
        is_kernel_partition ?
        manifest_.kernel_install_operations(
            next_operation_num_ - num_rootfs_operations_) :
        manifest_.install_operations(next_operation_num_);

    CopyDataToBuffer(&c_bytes, &count, op.data_length());

    // Check whether we received all of the next operation's data payload.
    if (!CanPerformInstallOperation(op))
      return true;

    // Validate the operation only if the metadata signature is present.
    // Otherwise, keep the old behavior. This serves as a knob to disable
    // the validation logic in case we find some regression after rollout.
    // NOTE: If hash checks are mandatory and if metadata_signature is empty,
    // we would have already failed in ParsePayloadMetadata method and thus not
    // even be here. So no need to handle that case again here.
    if (!install_plan_->metadata_signature.empty()) {
      // Note: Validate must be called only if CanPerformInstallOperation is
      // called. Otherwise, we might be failing operations before even if there
      // isn't sufficient data to compute the proper hash.
      *error = ValidateOperationHash(op);
      if (*error != ErrorCode::kSuccess) {
        if (install_plan_->hash_checks_mandatory) {
          LOG(ERROR) << "Mandatory operation hash check failed";
          return false;
        }

        // For non-mandatory cases, just send a UMA stat.
        LOG(WARNING) << "Ignoring operation validation errors";
        SendUmaStat(*error);
        *error = ErrorCode::kSuccess;
      }
    }

    // Makes sure we unblock exit when this operation completes.
    ScopedTerminatorExitUnblocker exit_unblocker =
        ScopedTerminatorExitUnblocker();  // Avoids a compiler unused var bug.

    bool op_result;
    if (op.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE ||
        op.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ)
      op_result = HandleOpResult(
          PerformReplaceOperation(op, is_kernel_partition), "replace", error);
    else if (op.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE)
      op_result = HandleOpResult(
          PerformMoveOperation(op, is_kernel_partition), "move", error);
    else if (op.type() == DeltaArchiveManifest_InstallOperation_Type_BSDIFF)
      op_result = HandleOpResult(
          PerformBsdiffOperation(op, is_kernel_partition), "bsdiff", error);
    else if (op.type() ==
             DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY)
      op_result =
          HandleOpResult(PerformSourceCopyOperation(op, is_kernel_partition),
                         "source_copy", error);
    else if (op.type() ==
             DeltaArchiveManifest_InstallOperation_Type_SOURCE_BSDIFF)
      op_result =
          HandleOpResult(PerformSourceBsdiffOperation(op, is_kernel_partition),
                         "source_bsdiff", error);
    else
      op_result = HandleOpResult(false, "unknown", error);

    if (!op_result)
      return false;

    next_operation_num_++;
    UpdateOverallProgress(false, "Completed ");
    CheckpointUpdateProgress();
  }
  return true;
}

bool DeltaPerformer::IsManifestValid() {
  return manifest_valid_;
}

bool DeltaPerformer::CanPerformInstallOperation(
    const chromeos_update_engine::DeltaArchiveManifest_InstallOperation&
    operation) {
  // Move and source_copy operations don't require any data blob, so they can
  // always be performed.
  if (operation.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE ||
      operation.type() ==
          DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY)
    return true;

  // See if we have the entire data blob in the buffer
  if (operation.data_offset() < buffer_offset_) {
    LOG(ERROR) << "we threw away data it seems?";
    return false;
  }

  return (operation.data_offset() + operation.data_length() <=
          buffer_offset_ + buffer_.size());
}

bool DeltaPerformer::PerformReplaceOperation(
    const DeltaArchiveManifest_InstallOperation& operation,
    bool is_kernel_partition) {
  CHECK(operation.type() == \
        DeltaArchiveManifest_InstallOperation_Type_REPLACE || \
        operation.type() == \
        DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);

  // Since we delete data off the beginning of the buffer as we use it,
  // the data we need should be exactly at the beginning of the buffer.
  TEST_AND_RETURN_FALSE(buffer_offset_ == operation.data_offset());
  TEST_AND_RETURN_FALSE(buffer_.size() >= operation.data_length());

  // Extract the signature message if it's in this operation.
  ExtractSignatureMessage(operation);

  DirectExtentWriter direct_writer;
  ZeroPadExtentWriter zero_pad_writer(&direct_writer);
  unique_ptr<BzipExtentWriter> bzip_writer;

  // Since bzip decompression is optional, we have a variable writer that will
  // point to one of the ExtentWriter objects above.
  ExtentWriter* writer = nullptr;
  if (operation.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE) {
    writer = &zero_pad_writer;
  } else if (operation.type() ==
             DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ) {
    bzip_writer.reset(new BzipExtentWriter(&zero_pad_writer));
    writer = bzip_writer.get();
  } else {
    NOTREACHED();
  }

  // Create a vector of extents to pass to the ExtentWriter.
  vector<Extent> extents;
  for (int i = 0; i < operation.dst_extents_size(); i++) {
    extents.push_back(operation.dst_extents(i));
  }

  FileDescriptorPtr fd = is_kernel_partition ? kernel_fd_ : fd_;

  TEST_AND_RETURN_FALSE(writer->Init(fd, extents, block_size_));
  TEST_AND_RETURN_FALSE(writer->Write(buffer_.data(), operation.data_length()));
  TEST_AND_RETURN_FALSE(writer->End());

  // Update buffer
  DiscardBuffer(true);
  return true;
}

bool DeltaPerformer::PerformMoveOperation(
    const DeltaArchiveManifest_InstallOperation& operation,
    bool is_kernel_partition) {
  // Calculate buffer size. Note, this function doesn't do a sliding
  // window to copy in case the source and destination blocks overlap.
  // If we wanted to do a sliding window, we could program the server
  // to generate deltas that effectively did a sliding window.

  uint64_t blocks_to_read = 0;
  for (int i = 0; i < operation.src_extents_size(); i++)
    blocks_to_read += operation.src_extents(i).num_blocks();

  uint64_t blocks_to_write = 0;
  for (int i = 0; i < operation.dst_extents_size(); i++)
    blocks_to_write += operation.dst_extents(i).num_blocks();

  DCHECK_EQ(blocks_to_write, blocks_to_read);
  chromeos::Blob buf(blocks_to_write * block_size_);

  FileDescriptorPtr fd = is_kernel_partition ? kernel_fd_ : fd_;

  // Read in bytes.
  ssize_t bytes_read = 0;
  for (int i = 0; i < operation.src_extents_size(); i++) {
    ssize_t bytes_read_this_iteration = 0;
    const Extent& extent = operation.src_extents(i);
    const size_t bytes = extent.num_blocks() * block_size_;
    TEST_AND_RETURN_FALSE(extent.start_block() != kSparseHole);
    TEST_AND_RETURN_FALSE(utils::PReadAll(fd,
                                          &buf[bytes_read],
                                          bytes,
                                          extent.start_block() * block_size_,
                                          &bytes_read_this_iteration));
    TEST_AND_RETURN_FALSE(
        bytes_read_this_iteration == static_cast<ssize_t>(bytes));
    bytes_read += bytes_read_this_iteration;
  }

  // Write bytes out.
  ssize_t bytes_written = 0;
  for (int i = 0; i < operation.dst_extents_size(); i++) {
    const Extent& extent = operation.dst_extents(i);
    const size_t bytes = extent.num_blocks() * block_size_;
    TEST_AND_RETURN_FALSE(extent.start_block() != kSparseHole);
    TEST_AND_RETURN_FALSE(utils::PWriteAll(fd,
                                           &buf[bytes_written],
                                           bytes,
                                           extent.start_block() * block_size_));
    bytes_written += bytes;
  }
  DCHECK_EQ(bytes_written, bytes_read);
  DCHECK_EQ(bytes_written, static_cast<ssize_t>(buf.size()));
  return true;
}

namespace {

// Takes |extents| and fills an empty vector |blocks| with a block index for
// each block in |extents|. For example, [(3, 2), (8, 1)] would give [3, 4, 8].
void ExtentsToBlocks(const RepeatedPtrField<Extent>& extents,
                     vector<uint64_t>* blocks) {
  for (Extent ext : extents) {
    for (uint64_t j = 0; j < ext.num_blocks(); j++)
      blocks->push_back(ext.start_block() + j);
  }
}

// Takes |extents| and returns the number of blocks in those extents.
uint64_t GetBlockCount(const RepeatedPtrField<Extent>& extents) {
  uint64_t sum = 0;
  for (Extent ext : extents) {
    sum += ext.num_blocks();
  }
  return sum;
}

}  // namespace

bool DeltaPerformer::PerformSourceCopyOperation(
    const DeltaArchiveManifest_InstallOperation& operation,
    bool is_kernel_partition) {
  if (operation.has_src_length())
    TEST_AND_RETURN_FALSE(operation.src_length() % block_size_ == 0);
  if (operation.has_dst_length())
    TEST_AND_RETURN_FALSE(operation.dst_length() % block_size_ == 0);

  uint64_t blocks_to_read = GetBlockCount(operation.src_extents());
  uint64_t blocks_to_write = GetBlockCount(operation.dst_extents());
  TEST_AND_RETURN_FALSE(blocks_to_write ==  blocks_to_read);

  // Create vectors of all the individual src/dst blocks.
  vector<uint64_t> src_blocks;
  vector<uint64_t> dst_blocks;
  ExtentsToBlocks(operation.src_extents(), &src_blocks);
  ExtentsToBlocks(operation.dst_extents(), &dst_blocks);
  DCHECK_EQ(src_blocks.size(), blocks_to_read);
  DCHECK_EQ(src_blocks.size(), dst_blocks.size());

  FileDescriptorPtr src_fd =
      is_kernel_partition ? source_kernel_fd_ : source_fd_;
  FileDescriptorPtr dst_fd = is_kernel_partition? kernel_fd_ : fd_;

  chromeos::Blob buf(block_size_);
  ssize_t bytes_read = 0;
  // Read/write one block at a time.
  for (uint64_t i = 0; i < blocks_to_read; i++) {
    ssize_t bytes_read_this_iteration = 0;
    uint64_t src_block = src_blocks[i];
    uint64_t dst_block = dst_blocks[i];

    // Read in bytes.
    TEST_AND_RETURN_FALSE(
        utils::PReadAll(src_fd,
                        buf.data(),
                        block_size_,
                        src_block * block_size_,
                        &bytes_read_this_iteration));

    // Write bytes out.
    TEST_AND_RETURN_FALSE(
        utils::PWriteAll(dst_fd,
                         buf.data(),
                         block_size_,
                         dst_block * block_size_));

    bytes_read += bytes_read_this_iteration;
    TEST_AND_RETURN_FALSE(bytes_read_this_iteration ==
                          static_cast<ssize_t>(block_size_));
  }
  DCHECK_EQ(bytes_read, static_cast<ssize_t>(blocks_to_read * block_size_));
  return true;
}

bool DeltaPerformer::ExtentsToBsdiffPositionsString(
    const RepeatedPtrField<Extent>& extents,
    uint64_t block_size,
    uint64_t full_length,
    string* positions_string) {
  string ret;
  uint64_t length = 0;
  for (int i = 0; i < extents.size(); i++) {
    Extent extent = extents.Get(i);
    int64_t start = extent.start_block() * block_size;
    uint64_t this_length = min(full_length - length,
                               extent.num_blocks() * block_size);
    ret += base::StringPrintf("%" PRIi64 ":%" PRIu64 ",", start, this_length);
    length += this_length;
  }
  TEST_AND_RETURN_FALSE(length == full_length);
  if (!ret.empty())
    ret.resize(ret.size() - 1);  // Strip trailing comma off
  *positions_string = ret;
  return true;
}

bool DeltaPerformer::PerformBsdiffOperation(
    const DeltaArchiveManifest_InstallOperation& operation,
    bool is_kernel_partition) {
  // Since we delete data off the beginning of the buffer as we use it,
  // the data we need should be exactly at the beginning of the buffer.
  TEST_AND_RETURN_FALSE(buffer_offset_ == operation.data_offset());
  TEST_AND_RETURN_FALSE(buffer_.size() >= operation.data_length());

  string input_positions;
  TEST_AND_RETURN_FALSE(ExtentsToBsdiffPositionsString(operation.src_extents(),
                                                       block_size_,
                                                       operation.src_length(),
                                                       &input_positions));
  string output_positions;
  TEST_AND_RETURN_FALSE(ExtentsToBsdiffPositionsString(operation.dst_extents(),
                                                       block_size_,
                                                       operation.dst_length(),
                                                       &output_positions));

  string temp_filename;
  TEST_AND_RETURN_FALSE(utils::MakeTempFile("/tmp/au_patch.XXXXXX",
                                            &temp_filename,
                                            nullptr));
  ScopedPathUnlinker path_unlinker(temp_filename);
  {
    int fd = open(temp_filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ScopedFdCloser fd_closer(&fd);
    TEST_AND_RETURN_FALSE(
        utils::WriteAll(fd, buffer_.data(), operation.data_length()));
  }

  // Update the buffer to release the patch data memory as soon as the patch
  // file is written out.
  DiscardBuffer(true);

  const string& path = is_kernel_partition ? kernel_path_ : path_;
  vector<string> cmd{kBspatchPath, path, path, temp_filename,
                     input_positions, output_positions};

  int return_code = 0;
  TEST_AND_RETURN_FALSE(
      Subprocess::SynchronousExecFlags(cmd,
                                       G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
                                       &return_code,
                                       nullptr));
  TEST_AND_RETURN_FALSE(return_code == 0);

  if (operation.dst_length() % block_size_) {
    // Zero out rest of final block.
    // TODO(adlr): build this into bspatch; it's more efficient that way.
    const Extent& last_extent =
        operation.dst_extents(operation.dst_extents_size() - 1);
    const uint64_t end_byte =
        (last_extent.start_block() + last_extent.num_blocks()) * block_size_;
    const uint64_t begin_byte =
        end_byte - (block_size_ - operation.dst_length() % block_size_);
    chromeos::Blob zeros(end_byte - begin_byte);
    FileDescriptorPtr fd = is_kernel_partition ? kernel_fd_ : fd_;
    TEST_AND_RETURN_FALSE(
        utils::PWriteAll(fd, zeros.data(), end_byte - begin_byte, begin_byte));
  }
  return true;
}

bool DeltaPerformer::PerformSourceBsdiffOperation(
    const DeltaArchiveManifest_InstallOperation& operation,
    bool is_kernel_partition) {
  // Since we delete data off the beginning of the buffer as we use it,
  // the data we need should be exactly at the beginning of the buffer.
  TEST_AND_RETURN_FALSE(buffer_offset_ == operation.data_offset());
  TEST_AND_RETURN_FALSE(buffer_.size() >= operation.data_length());
  if (operation.has_src_length())
    TEST_AND_RETURN_FALSE(operation.src_length() % block_size_ == 0);
  if (operation.has_dst_length())
    TEST_AND_RETURN_FALSE(operation.dst_length() % block_size_ == 0);

  string input_positions;
  TEST_AND_RETURN_FALSE(ExtentsToBsdiffPositionsString(operation.src_extents(),
                                                       block_size_,
                                                       operation.src_length(),
                                                       &input_positions));
  string output_positions;
  TEST_AND_RETURN_FALSE(ExtentsToBsdiffPositionsString(operation.dst_extents(),
                                                       block_size_,
                                                       operation.dst_length(),
                                                       &output_positions));

  string temp_filename;
  TEST_AND_RETURN_FALSE(utils::MakeTempFile("/tmp/au_patch.XXXXXX",
                                            &temp_filename,
                                            nullptr));
  ScopedPathUnlinker path_unlinker(temp_filename);
  {
    int fd = open(temp_filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ScopedFdCloser fd_closer(&fd);
    TEST_AND_RETURN_FALSE(
        utils::WriteAll(fd, buffer_.data(), operation.data_length()));
  }

  // Update the buffer to release the patch data memory as soon as the patch
  // file is written out.
  DiscardBuffer(true);

  const string& src_path = is_kernel_partition ?
                           install_plan_->kernel_source_path :
                           install_plan_->source_path;
  const string& dst_path = is_kernel_partition ? kernel_path_ : path_;
  vector<string> cmd{kBspatchPath, src_path, dst_path, temp_filename,
                     input_positions, output_positions};

  int return_code = 0;
  TEST_AND_RETURN_FALSE(
      Subprocess::SynchronousExecFlags(cmd,
                                       G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
                                       &return_code,
                                       nullptr));
  TEST_AND_RETURN_FALSE(return_code == 0);
  return true;
}

bool DeltaPerformer::ExtractSignatureMessage(
    const DeltaArchiveManifest_InstallOperation& operation) {
  if (operation.type() != DeltaArchiveManifest_InstallOperation_Type_REPLACE ||
      !manifest_.has_signatures_offset() ||
      manifest_.signatures_offset() != operation.data_offset()) {
    return false;
  }
  TEST_AND_RETURN_FALSE(manifest_.has_signatures_size() &&
                        manifest_.signatures_size() == operation.data_length());
  TEST_AND_RETURN_FALSE(signatures_message_data_.empty());
  TEST_AND_RETURN_FALSE(buffer_offset_ == manifest_.signatures_offset());
  TEST_AND_RETURN_FALSE(buffer_.size() >= manifest_.signatures_size());
  signatures_message_data_.assign(
      buffer_.begin(),
      buffer_.begin() + manifest_.signatures_size());

  // Save the signature blob because if the update is interrupted after the
  // download phase we don't go through this path anymore. Some alternatives to
  // consider:
  //
  // 1. On resume, re-download the signature blob from the server and re-verify
  // it.
  //
  // 2. Verify the signature as soon as it's received and don't checkpoint the
  // blob and the signed sha-256 context.
  LOG_IF(WARNING, !prefs_->SetString(kPrefsUpdateStateSignatureBlob,
                                     string(signatures_message_data_.begin(),
                                            signatures_message_data_.end())))
      << "Unable to store the signature blob.";
  // The hash of all data consumed so far should be verified against the signed
  // hash.
  signed_hash_context_ = hash_calculator_.GetContext();
  LOG_IF(WARNING, !prefs_->SetString(kPrefsUpdateStateSignedSHA256Context,
                                     signed_hash_context_))
      << "Unable to store the signed hash context.";
  LOG(INFO) << "Extracted signature data of size "
            << manifest_.signatures_size() << " at "
            << manifest_.signatures_offset();
  return true;
}

bool DeltaPerformer::GetPublicKeyFromResponse(base::FilePath *out_tmp_key) {
  if (system_state_->hardware()->IsOfficialBuild() ||
      utils::FileExists(public_key_path_.c_str()) ||
      install_plan_->public_key_rsa.empty())
    return false;

  if (!utils::DecodeAndStoreBase64String(install_plan_->public_key_rsa,
                                         out_tmp_key))
    return false;

  return true;
}

ErrorCode DeltaPerformer::ValidateMetadataSignature(
    const void* metadata, uint64_t metadata_size) {

  if (install_plan_->metadata_signature.empty()) {
    if (install_plan_->hash_checks_mandatory) {
      LOG(ERROR) << "Missing mandatory metadata signature in Omaha response";
      return ErrorCode::kDownloadMetadataSignatureMissingError;
    }

    // For non-mandatory cases, just send a UMA stat.
    LOG(WARNING) << "Cannot validate metadata as the signature is empty";
    SendUmaStat(ErrorCode::kDownloadMetadataSignatureMissingError);
    return ErrorCode::kSuccess;
  }

  // Convert base64-encoded signature to raw bytes.
  chromeos::Blob metadata_signature;
  if (!chromeos::data_encoding::Base64Decode(install_plan_->metadata_signature,
                                             &metadata_signature)) {
    LOG(ERROR) << "Unable to decode base64 metadata signature: "
               << install_plan_->metadata_signature;
    return ErrorCode::kDownloadMetadataSignatureError;
  }

  // See if we should use the public RSA key in the Omaha response.
  base::FilePath path_to_public_key(public_key_path_);
  base::FilePath tmp_key;
  if (GetPublicKeyFromResponse(&tmp_key))
    path_to_public_key = tmp_key;
  ScopedPathUnlinker tmp_key_remover(tmp_key.value());
  if (tmp_key.empty())
    tmp_key_remover.set_should_remove(false);

  LOG(INFO) << "Verifying metadata hash signature using public key: "
            << path_to_public_key.value();

  chromeos::Blob expected_metadata_hash;
  if (!PayloadVerifier::GetRawHashFromSignature(metadata_signature,
                                                path_to_public_key.value(),
                                                &expected_metadata_hash)) {
    LOG(ERROR) << "Unable to compute expected hash from metadata signature";
    return ErrorCode::kDownloadMetadataSignatureError;
  }

  OmahaHashCalculator metadata_hasher;
  metadata_hasher.Update(metadata, metadata_size);
  if (!metadata_hasher.Finalize()) {
    LOG(ERROR) << "Unable to compute actual hash of manifest";
    return ErrorCode::kDownloadMetadataSignatureVerificationError;
  }

  chromeos::Blob calculated_metadata_hash = metadata_hasher.raw_hash();
  PayloadVerifier::PadRSA2048SHA256Hash(&calculated_metadata_hash);
  if (calculated_metadata_hash.empty()) {
    LOG(ERROR) << "Computed actual hash of metadata is empty.";
    return ErrorCode::kDownloadMetadataSignatureVerificationError;
  }

  if (calculated_metadata_hash != expected_metadata_hash) {
    LOG(ERROR) << "Manifest hash verification failed. Expected hash = ";
    utils::HexDumpVector(expected_metadata_hash);
    LOG(ERROR) << "Calculated hash = ";
    utils::HexDumpVector(calculated_metadata_hash);
    return ErrorCode::kDownloadMetadataSignatureMismatch;
  }

  // The autoupdate_CatchBadSignatures test checks for this string in
  // log-files. Keep in sync.
  LOG(INFO) << "Metadata hash signature matches value in Omaha response.";
  return ErrorCode::kSuccess;
}

ErrorCode DeltaPerformer::ValidateManifest() {
  // Perform assorted checks to sanity check the manifest, make sure it
  // matches data from other sources, and that it is a supported version.
  //
  // TODO(garnold) in general, the presence of an old partition hash should be
  // the sole indicator for a delta update, as we would generally like update
  // payloads to be self contained and not assume an Omaha response to tell us
  // that. However, since this requires some massive reengineering of the update
  // flow (making filesystem copying happen conditionally only *after*
  // downloading and parsing of the update manifest) we'll put it off for now.
  // See chromium-os:7597 for further discussion.
  if (install_plan_->is_full_update) {
    if (manifest_.has_old_kernel_info() || manifest_.has_old_rootfs_info()) {
      LOG(ERROR) << "Purported full payload contains old partition "
                    "hash(es), aborting update";
      return ErrorCode::kPayloadMismatchedType;
    }

    if (manifest_.minor_version() != kFullPayloadMinorVersion) {
      LOG(ERROR) << "Manifest contains minor version "
                 << manifest_.minor_version()
                 << ", but all full payloads should have version "
                 << kFullPayloadMinorVersion << ".";
      return ErrorCode::kUnsupportedMinorPayloadVersion;
    }
  } else {
    if (manifest_.minor_version() != supported_minor_version_) {
      LOG(ERROR) << "Manifest contains minor version "
                 << manifest_.minor_version()
                 << " not the supported "
                 << supported_minor_version_;
      return ErrorCode::kUnsupportedMinorPayloadVersion;
    }
  }

  // TODO(garnold) we should be adding more and more manifest checks, such as
  // partition boundaries etc (see chromium-os:37661).

  return ErrorCode::kSuccess;
}

ErrorCode DeltaPerformer::ValidateOperationHash(
    const DeltaArchiveManifest_InstallOperation& operation) {

  if (!operation.data_sha256_hash().size()) {
    if (!operation.data_length()) {
      // Operations that do not have any data blob won't have any operation hash
      // either. So, these operations are always considered validated since the
      // metadata that contains all the non-data-blob portions of the operation
      // has already been validated. This is true for both HTTP and HTTPS cases.
      return ErrorCode::kSuccess;
    }

    // No hash is present for an operation that has data blobs. This shouldn't
    // happen normally for any client that has this code, because the
    // corresponding update should have been produced with the operation
    // hashes. So if it happens it means either we've turned operation hash
    // generation off in DeltaDiffGenerator or it's a regression of some sort.
    // One caveat though: The last operation is a dummy signature operation
    // that doesn't have a hash at the time the manifest is created. So we
    // should not complaint about that operation. This operation can be
    // recognized by the fact that it's offset is mentioned in the manifest.
    if (manifest_.signatures_offset() &&
        manifest_.signatures_offset() == operation.data_offset()) {
      LOG(INFO) << "Skipping hash verification for signature operation "
                << next_operation_num_ + 1;
    } else {
      if (install_plan_->hash_checks_mandatory) {
        LOG(ERROR) << "Missing mandatory operation hash for operation "
                   << next_operation_num_ + 1;
        return ErrorCode::kDownloadOperationHashMissingError;
      }

      // For non-mandatory cases, just send a UMA stat.
      LOG(WARNING) << "Cannot validate operation " << next_operation_num_ + 1
                   << " as there's no operation hash in manifest";
      SendUmaStat(ErrorCode::kDownloadOperationHashMissingError);
    }
    return ErrorCode::kSuccess;
  }

  chromeos::Blob expected_op_hash;
  expected_op_hash.assign(operation.data_sha256_hash().data(),
                          (operation.data_sha256_hash().data() +
                           operation.data_sha256_hash().size()));

  OmahaHashCalculator operation_hasher;
  operation_hasher.Update(buffer_.data(), operation.data_length());
  if (!operation_hasher.Finalize()) {
    LOG(ERROR) << "Unable to compute actual hash of operation "
               << next_operation_num_;
    return ErrorCode::kDownloadOperationHashVerificationError;
  }

  chromeos::Blob calculated_op_hash = operation_hasher.raw_hash();
  if (calculated_op_hash != expected_op_hash) {
    LOG(ERROR) << "Hash verification failed for operation "
               << next_operation_num_ << ". Expected hash = ";
    utils::HexDumpVector(expected_op_hash);
    LOG(ERROR) << "Calculated hash over " << operation.data_length()
               << " bytes at offset: " << operation.data_offset() << " = ";
    utils::HexDumpVector(calculated_op_hash);
    return ErrorCode::kDownloadOperationHashMismatch;
  }

  return ErrorCode::kSuccess;
}

#define TEST_AND_RETURN_VAL(_retval, _condition)                \
  do {                                                          \
    if (!(_condition)) {                                        \
      LOG(ERROR) << "VerifyPayload failure: " << #_condition;   \
      return _retval;                                           \
    }                                                           \
  } while (0);

ErrorCode DeltaPerformer::VerifyPayload(
    const string& update_check_response_hash,
    const uint64_t update_check_response_size) {

  // See if we should use the public RSA key in the Omaha response.
  base::FilePath path_to_public_key(public_key_path_);
  base::FilePath tmp_key;
  if (GetPublicKeyFromResponse(&tmp_key))
    path_to_public_key = tmp_key;
  ScopedPathUnlinker tmp_key_remover(tmp_key.value());
  if (tmp_key.empty())
    tmp_key_remover.set_should_remove(false);

  LOG(INFO) << "Verifying payload using public key: "
            << path_to_public_key.value();

  // Verifies the download size.
  TEST_AND_RETURN_VAL(ErrorCode::kPayloadSizeMismatchError,
                      update_check_response_size ==
                      metadata_size_ + buffer_offset_);

  // Verifies the payload hash.
  const string& payload_hash_data = hash_calculator_.hash();
  TEST_AND_RETURN_VAL(ErrorCode::kDownloadPayloadVerificationError,
                      !payload_hash_data.empty());
  TEST_AND_RETURN_VAL(ErrorCode::kPayloadHashMismatchError,
                      payload_hash_data == update_check_response_hash);

  // Verifies the signed payload hash.
  if (!utils::FileExists(path_to_public_key.value().c_str())) {
    LOG(WARNING) << "Not verifying signed delta payload -- missing public key.";
    return ErrorCode::kSuccess;
  }
  TEST_AND_RETURN_VAL(ErrorCode::kSignedDeltaPayloadExpectedError,
                      !signatures_message_data_.empty());
  chromeos::Blob signed_hash_data;
  TEST_AND_RETURN_VAL(ErrorCode::kDownloadPayloadPubKeyVerificationError,
                      PayloadVerifier::VerifySignature(
                          signatures_message_data_,
                          path_to_public_key.value(),
                          &signed_hash_data));
  OmahaHashCalculator signed_hasher;
  TEST_AND_RETURN_VAL(ErrorCode::kDownloadPayloadPubKeyVerificationError,
                      signed_hasher.SetContext(signed_hash_context_));
  TEST_AND_RETURN_VAL(ErrorCode::kDownloadPayloadPubKeyVerificationError,
                      signed_hasher.Finalize());
  chromeos::Blob hash_data = signed_hasher.raw_hash();
  PayloadVerifier::PadRSA2048SHA256Hash(&hash_data);
  TEST_AND_RETURN_VAL(ErrorCode::kDownloadPayloadPubKeyVerificationError,
                      !hash_data.empty());
  if (hash_data != signed_hash_data) {
    // The autoupdate_CatchBadSignatures test checks for this string
    // in log-files. Keep in sync.
    LOG(ERROR) << "Public key verification failed, thus update failed. "
        "Attached Signature:";
    utils::HexDumpVector(signed_hash_data);
    LOG(ERROR) << "Computed Signature:";
    utils::HexDumpVector(hash_data);
    return ErrorCode::kDownloadPayloadPubKeyVerificationError;
  }

  LOG(INFO) << "Payload hash matches value in payload.";

  // At this point, we are guaranteed to have downloaded a full payload, i.e
  // the one whose size matches the size mentioned in Omaha response. If any
  // errors happen after this, it's likely a problem with the payload itself or
  // the state of the system and not a problem with the URL or network.  So,
  // indicate that to the payload state so that AU can backoff appropriately.
  system_state_->payload_state()->DownloadComplete();

  return ErrorCode::kSuccess;
}

bool DeltaPerformer::GetNewPartitionInfo(uint64_t* kernel_size,
                                         chromeos::Blob* kernel_hash,
                                         uint64_t* rootfs_size,
                                         chromeos::Blob* rootfs_hash) {
  TEST_AND_RETURN_FALSE(manifest_valid_ &&
                        manifest_.has_new_kernel_info() &&
                        manifest_.has_new_rootfs_info());
  *kernel_size = manifest_.new_kernel_info().size();
  *rootfs_size = manifest_.new_rootfs_info().size();
  chromeos::Blob new_kernel_hash(manifest_.new_kernel_info().hash().begin(),
                                 manifest_.new_kernel_info().hash().end());
  chromeos::Blob new_rootfs_hash(manifest_.new_rootfs_info().hash().begin(),
                                 manifest_.new_rootfs_info().hash().end());
  kernel_hash->swap(new_kernel_hash);
  rootfs_hash->swap(new_rootfs_hash);
  return true;
}

namespace {
void LogVerifyError(bool is_kern,
                    const string& local_hash,
                    const string& expected_hash) {
  const char* type = is_kern ? "kernel" : "rootfs";
  LOG(ERROR) << "This is a server-side error due to "
             << "mismatched delta update image!";
  LOG(ERROR) << "The delta I've been given contains a " << type << " delta "
             << "update that must be applied over a " << type << " with "
             << "a specific checksum, but the " << type << " we're starting "
             << "with doesn't have that checksum! This means that "
             << "the delta I've been given doesn't match my existing "
             << "system. The " << type << " partition I have has hash: "
             << local_hash << " but the update expected me to have "
             << expected_hash << " .";
  if (is_kern) {
    LOG(INFO) << "To get the checksum of a kernel partition on a "
              << "booted machine, run this command (change /dev/sda2 "
              << "as needed): dd if=/dev/sda2 bs=1M 2>/dev/null | "
              << "openssl dgst -sha256 -binary | openssl base64";
  } else {
    LOG(INFO) << "To get the checksum of a rootfs partition on a "
              << "booted machine, run this command (change /dev/sda3 "
              << "as needed): dd if=/dev/sda3 bs=1M count=$(( "
              << "$(dumpe2fs /dev/sda3  2>/dev/null | grep 'Block count' "
              << "| sed 's/[^0-9]*//') / 256 )) | "
              << "openssl dgst -sha256 -binary | openssl base64";
  }
  LOG(INFO) << "To get the checksum of partitions in a bin file, "
            << "run: .../src/scripts/sha256_partitions.sh .../file.bin";
}

string StringForHashBytes(const void* bytes, size_t size) {
  return chromeos::data_encoding::Base64Encode(bytes, size);
}
}  // namespace

bool DeltaPerformer::VerifySourcePartitions() {
  LOG(INFO) << "Verifying source partitions.";
  CHECK(manifest_valid_);
  CHECK(install_plan_);
  if (manifest_.has_old_kernel_info()) {
    const PartitionInfo& info = manifest_.old_kernel_info();
    bool valid =
        !install_plan_->source_kernel_hash.empty() &&
        install_plan_->source_kernel_hash.size() == info.hash().size() &&
        memcmp(install_plan_->source_kernel_hash.data(),
               info.hash().data(),
               install_plan_->source_kernel_hash.size()) == 0;
    if (!valid) {
      LogVerifyError(true,
                     StringForHashBytes(
                         install_plan_->source_kernel_hash.data(),
                         install_plan_->source_kernel_hash.size()),
                     StringForHashBytes(info.hash().data(),
                                        info.hash().size()));
    }
    TEST_AND_RETURN_FALSE(valid);
  }
  if (manifest_.has_old_rootfs_info()) {
    const PartitionInfo& info = manifest_.old_rootfs_info();
    bool valid =
        !install_plan_->source_rootfs_hash.empty() &&
        install_plan_->source_rootfs_hash.size() == info.hash().size() &&
        memcmp(install_plan_->source_rootfs_hash.data(),
               info.hash().data(),
               install_plan_->source_rootfs_hash.size()) == 0;
    if (!valid) {
      LogVerifyError(false,
                     StringForHashBytes(
                         install_plan_->source_rootfs_hash.data(),
                         install_plan_->source_rootfs_hash.size()),
                     StringForHashBytes(info.hash().data(),
                                        info.hash().size()));
    }
    TEST_AND_RETURN_FALSE(valid);
  }
  return true;
}

void DeltaPerformer::DiscardBuffer(bool do_advance_offset) {
  // Update the buffer offset.
  if (do_advance_offset)
    buffer_offset_ += buffer_.size();

  // Hash the content.
  hash_calculator_.Update(buffer_.data(), buffer_.size());

  // Swap content with an empty vector to ensure that all memory is released.
  chromeos::Blob().swap(buffer_);
}

bool DeltaPerformer::CanResumeUpdate(PrefsInterface* prefs,
                                     string update_check_response_hash) {
  int64_t next_operation = kUpdateStateOperationInvalid;
  if (!(prefs->GetInt64(kPrefsUpdateStateNextOperation, &next_operation) &&
        next_operation != kUpdateStateOperationInvalid &&
        next_operation > 0))
    return false;

  string interrupted_hash;
  if (!(prefs->GetString(kPrefsUpdateCheckResponseHash, &interrupted_hash) &&
        !interrupted_hash.empty() &&
        interrupted_hash == update_check_response_hash))
    return false;

  int64_t resumed_update_failures;
  if (!(prefs->GetInt64(kPrefsResumedUpdateFailures, &resumed_update_failures)
        && resumed_update_failures > kMaxResumedUpdateFailures))
    return false;

  // Sanity check the rest.
  int64_t next_data_offset = -1;
  if (!(prefs->GetInt64(kPrefsUpdateStateNextDataOffset, &next_data_offset) &&
        next_data_offset >= 0))
    return false;

  string sha256_context;
  if (!(prefs->GetString(kPrefsUpdateStateSHA256Context, &sha256_context) &&
        !sha256_context.empty()))
    return false;

  int64_t manifest_metadata_size = 0;
  if (!(prefs->GetInt64(kPrefsManifestMetadataSize, &manifest_metadata_size) &&
        manifest_metadata_size > 0))
    return false;

  return true;
}

bool DeltaPerformer::ResetUpdateProgress(PrefsInterface* prefs, bool quick) {
  TEST_AND_RETURN_FALSE(prefs->SetInt64(kPrefsUpdateStateNextOperation,
                                        kUpdateStateOperationInvalid));
  if (!quick) {
    prefs->SetString(kPrefsUpdateCheckResponseHash, "");
    prefs->SetInt64(kPrefsUpdateStateNextDataOffset, -1);
    prefs->SetInt64(kPrefsUpdateStateNextDataLength, 0);
    prefs->SetString(kPrefsUpdateStateSHA256Context, "");
    prefs->SetString(kPrefsUpdateStateSignedSHA256Context, "");
    prefs->SetString(kPrefsUpdateStateSignatureBlob, "");
    prefs->SetInt64(kPrefsManifestMetadataSize, -1);
    prefs->SetInt64(kPrefsResumedUpdateFailures, 0);
  }
  return true;
}

bool DeltaPerformer::CheckpointUpdateProgress() {
  Terminator::set_exit_blocked(true);
  if (last_updated_buffer_offset_ != buffer_offset_) {
    // Resets the progress in case we die in the middle of the state update.
    ResetUpdateProgress(prefs_, true);
    TEST_AND_RETURN_FALSE(
        prefs_->SetString(kPrefsUpdateStateSHA256Context,
                          hash_calculator_.GetContext()));
    TEST_AND_RETURN_FALSE(prefs_->SetInt64(kPrefsUpdateStateNextDataOffset,
                                           buffer_offset_));
    last_updated_buffer_offset_ = buffer_offset_;

    if (next_operation_num_ < num_total_operations_) {
      const bool is_kernel_partition =
          next_operation_num_ >= num_rootfs_operations_;
      const DeltaArchiveManifest_InstallOperation &op =
          is_kernel_partition ?
          manifest_.kernel_install_operations(
              next_operation_num_ - num_rootfs_operations_) :
          manifest_.install_operations(next_operation_num_);
      TEST_AND_RETURN_FALSE(prefs_->SetInt64(kPrefsUpdateStateNextDataLength,
                                             op.data_length()));
    } else {
      TEST_AND_RETURN_FALSE(prefs_->SetInt64(kPrefsUpdateStateNextDataLength,
                                             0));
    }
  }
  TEST_AND_RETURN_FALSE(prefs_->SetInt64(kPrefsUpdateStateNextOperation,
                                         next_operation_num_));
  return true;
}

bool DeltaPerformer::PrimeUpdateState() {
  CHECK(manifest_valid_);
  block_size_ = manifest_.block_size();

  int64_t next_operation = kUpdateStateOperationInvalid;
  if (!prefs_->GetInt64(kPrefsUpdateStateNextOperation, &next_operation) ||
      next_operation == kUpdateStateOperationInvalid ||
      next_operation <= 0) {
    // Initiating a new update, no more state needs to be initialized.
    TEST_AND_RETURN_FALSE(VerifySourcePartitions());
    return true;
  }
  next_operation_num_ = next_operation;

  // Resuming an update -- load the rest of the update state.
  int64_t next_data_offset = -1;
  TEST_AND_RETURN_FALSE(prefs_->GetInt64(kPrefsUpdateStateNextDataOffset,
                                         &next_data_offset) &&
                        next_data_offset >= 0);
  buffer_offset_ = next_data_offset;

  // The signed hash context and the signature blob may be empty if the
  // interrupted update didn't reach the signature.
  prefs_->GetString(kPrefsUpdateStateSignedSHA256Context,
                    &signed_hash_context_);
  string signature_blob;
  if (prefs_->GetString(kPrefsUpdateStateSignatureBlob, &signature_blob)) {
    signatures_message_data_.assign(signature_blob.begin(),
                                    signature_blob.end());
  }

  string hash_context;
  TEST_AND_RETURN_FALSE(prefs_->GetString(kPrefsUpdateStateSHA256Context,
                                          &hash_context) &&
                        hash_calculator_.SetContext(hash_context));

  int64_t manifest_metadata_size = 0;
  TEST_AND_RETURN_FALSE(prefs_->GetInt64(kPrefsManifestMetadataSize,
                                         &manifest_metadata_size) &&
                        manifest_metadata_size > 0);
  metadata_size_ = manifest_metadata_size;

  // Advance the download progress to reflect what doesn't need to be
  // re-downloaded.
  total_bytes_received_ += buffer_offset_;

  // Speculatively count the resume as a failure.
  int64_t resumed_update_failures;
  if (prefs_->GetInt64(kPrefsResumedUpdateFailures, &resumed_update_failures)) {
    resumed_update_failures++;
  } else {
    resumed_update_failures = 1;
  }
  prefs_->SetInt64(kPrefsResumedUpdateFailures, resumed_update_failures);
  return true;
}

void DeltaPerformer::SendUmaStat(ErrorCode code) {
  utils::SendErrorCodeToUma(system_state_, code);
}

}  // namespace chromeos_update_engine
