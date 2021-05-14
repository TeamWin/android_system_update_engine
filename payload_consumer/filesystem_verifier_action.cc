//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/payload_consumer/filesystem_verifier_action.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/strings/string_util.h>
#include <brillo/data_encoding.h>
#include <brillo/message_loops/message_loop.h>
#include <brillo/secure_blob.h>
#include <brillo/streams/file_stream.h>

#include "common/error_code.h"
#include "payload_generator/delta_diff_generator.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/file_descriptor.h"

using brillo::data_encoding::Base64Encode;
using std::string;

// On a partition with verity enabled, we expect to see the following format:
// ===================================================
//              Normal Filesystem Data
// (this should take most of the space, like over 90%)
// ===================================================
//                  Hash tree
//         ~0.8% (e.g. 16M for 2GB image)
// ===================================================
//                  FEC data
//                    ~0.8%
// ===================================================
//                   Footer
//                     4K
// ===================================================

// For OTA that doesn't do on device verity computation, hash tree and fec data
// are written during DownloadAction as a regular InstallOp, so no special
// handling needed, we can just read the entire partition in 1 go.

// Verity enabled case: Only Normal FS data is written during download action.
// When hasing the entire partition, we will need to build the hash tree, write
// it to disk, then build FEC, and write it to disk. Therefore, it is important
// that we finish writing hash tree before we attempt to read & hash it. The
// same principal applies to FEC data.

// |verity_writer_| handles building and
// writing of FEC/HashTree, we just need to be careful when reading.
// Specifically, we must stop at beginning of Hash tree, let |verity_writer_|
// write both hash tree and FEC, then continue reading the remaining part of
// partition.

namespace chromeos_update_engine {

namespace {
const off_t kReadFileBufferSize = 128 * 1024;
constexpr float kVerityProgressPercent = 0.6;
}  // namespace

void FilesystemVerifierAction::PerformAction() {
  // Will tell the ActionProcessor we've failed if we return.
  ScopedActionCompleter abort_action_completer(processor_, this);

  if (!HasInputObject()) {
    LOG(ERROR) << "FilesystemVerifierAction missing input object.";
    return;
  }
  install_plan_ = GetInputObject();

  if (install_plan_.partitions.empty()) {
    LOG(INFO) << "No partitions to verify.";
    if (HasOutputPipe())
      SetOutputObject(install_plan_);
    abort_action_completer.set_code(ErrorCode::kSuccess);
    return;
  }
  install_plan_.Dump();
  StartPartitionHashing();
  abort_action_completer.set_should_complete(false);
}

void FilesystemVerifierAction::TerminateProcessing() {
  cancelled_ = true;
  Cleanup(ErrorCode::kSuccess);  // error code is ignored if canceled_ is true.
}

void FilesystemVerifierAction::Cleanup(ErrorCode code) {
  partition_fd_.reset();
  // This memory is not used anymore.
  buffer_.clear();

  // If we didn't write verity, partitions were maped. Releaase resource now.
  if (!install_plan_.write_verity &&
      dynamic_control_->UpdateUsesSnapshotCompression()) {
    LOG(INFO) << "Not writing verity and VABC is enabled, unmapping all "
                 "partitions";
    dynamic_control_->UnmapAllPartitions();
  }

  if (cancelled_)
    return;
  if (code == ErrorCode::kSuccess && HasOutputPipe())
    SetOutputObject(install_plan_);
  UpdateProgress(1.0);
  processor_->ActionComplete(this, code);
}

void FilesystemVerifierAction::UpdateProgress(double progress) {
  if (delegate_ != nullptr) {
    delegate_->OnVerifyProgressUpdate(progress);
  }
}

void FilesystemVerifierAction::UpdatePartitionProgress(double progress) {
  // We don't consider sizes of each partition. Every partition
  // has the same length on progress bar.
  // TODO(b/186087589): Take sizes of each partition into account.
  UpdateProgress((progress + partition_index_) /
                 install_plan_.partitions.size());
}

bool FilesystemVerifierAction::InitializeFdVABC(bool should_write_verity) {
  const InstallPlan::Partition& partition =
      install_plan_.partitions[partition_index_];

  if (!should_write_verity) {
    // In VABC, we cannot map/unmap partitions w/o first closing ALL fds first.
    // Since this function might be called inside a ScheduledTask, the closure
    // might have a copy of partition_fd_ when executing this function. Which
    // means even if we do |partition_fd_.reset()| here, there's a chance that
    // underlying fd isn't closed until we return. This is unacceptable, we need
    // to close |partition_fd| right away.
    if (partition_fd_) {
      partition_fd_->Close();
      partition_fd_.reset();
    }
    // In VABC, if we are not writing verity, just map all partitions,
    // and read using regular fd on |postinstall_mount_device| .
    // All read will go through snapuserd, which provides a consistent
    // view: device will use snapuserd to read partition during boot.
    // b/186196758
    // Call UnmapAllPartitions() first, because if we wrote verity before, these
    // writes won't be visible to previously opened snapuserd daemon. To ensure
    // that we will see the most up to date data from partitions, call Unmap()
    // then Map() to re-spin daemon.
    dynamic_control_->UnmapAllPartitions();
    dynamic_control_->MapAllPartitions();
    return InitializeFd(partition.readonly_target_path);
  }
  partition_fd_ =
      dynamic_control_->OpenCowFd(partition.name, partition.source_path, true);
  if (!partition_fd_) {
    LOG(ERROR) << "OpenCowReader(" << partition.name << ", "
               << partition.source_path << ") failed.";
    return false;
  }
  partition_size_ = partition.target_size;
  return true;
}

bool FilesystemVerifierAction::InitializeFd(const std::string& part_path) {
  partition_fd_ = FileDescriptorPtr(new EintrSafeFileDescriptor());
  const bool write_verity = ShouldWriteVerity();
  int flags = write_verity ? O_RDWR : O_RDONLY;
  if (!utils::SetBlockDeviceReadOnly(part_path, !write_verity)) {
    LOG(WARNING) << "Failed to set block device " << part_path << " as "
                 << (write_verity ? "writable" : "readonly");
  }
  if (!partition_fd_->Open(part_path.c_str(), flags)) {
    LOG(ERROR) << "Unable to open " << part_path << " for reading.";
    return false;
  }
  return true;
}

void FilesystemVerifierAction::WriteVerityAndHashPartition(
    FileDescriptorPtr fd,
    const off64_t start_offset,
    const off64_t end_offset,
    void* buffer,
    const size_t buffer_size) {
  if (start_offset >= end_offset) {
    LOG_IF(WARNING, start_offset > end_offset)
        << "start_offset is greater than end_offset : " << start_offset << " > "
        << end_offset;
    if (!verity_writer_->Finalize(fd, fd)) {
      LOG(ERROR) << "Failed to write verity data";
      Cleanup(ErrorCode::kVerityCalculationError);
      return;
    }
    if (dynamic_control_->UpdateUsesSnapshotCompression()) {
      // Spin up snapuserd to read fs.
      if (!InitializeFdVABC(false)) {
        LOG(ERROR) << "Failed to map all partitions";
        Cleanup(ErrorCode::kFilesystemVerifierError);
        return;
      }
    }
    HashPartition(partition_fd_, 0, partition_size_, buffer, buffer_size);
    return;
  }
  const auto cur_offset = fd->Seek(start_offset, SEEK_SET);
  if (cur_offset != start_offset) {
    PLOG(ERROR) << "Failed to seek to offset: " << start_offset;
    Cleanup(ErrorCode::kVerityCalculationError);
    return;
  }
  const auto read_size =
      std::min<size_t>(buffer_size, end_offset - start_offset);
  const auto bytes_read = fd->Read(buffer, read_size);
  if (bytes_read < 0 || static_cast<size_t>(bytes_read) != read_size) {
    PLOG(ERROR) << "Failed to read offset " << start_offset << " expected "
                << read_size << " bytes, actual: " << bytes_read;
    Cleanup(ErrorCode::kVerityCalculationError);
    return;
  }
  if (!verity_writer_->Update(
          start_offset, static_cast<const uint8_t*>(buffer), read_size)) {
    LOG(ERROR) << "VerityWriter::Update() failed";
    Cleanup(ErrorCode::kVerityCalculationError);
    return;
  }
  UpdatePartitionProgress((start_offset + bytes_read) * 1.0f / partition_size_ *
                          kVerityProgressPercent);
  CHECK(pending_task_id_.PostTask(
      FROM_HERE,
      base::BindOnce(&FilesystemVerifierAction::WriteVerityAndHashPartition,
                     base::Unretained(this),
                     fd,
                     start_offset + bytes_read,
                     end_offset,
                     buffer,
                     buffer_size)));
}

void FilesystemVerifierAction::HashPartition(FileDescriptorPtr fd,
                                             const off64_t start_offset,
                                             const off64_t end_offset,
                                             void* buffer,
                                             const size_t buffer_size) {
  if (start_offset >= end_offset) {
    LOG_IF(WARNING, start_offset > end_offset)
        << "start_offset is greater than end_offset : " << start_offset << " > "
        << end_offset;
    FinishPartitionHashing();
    return;
  }
  const auto cur_offset = fd->Seek(start_offset, SEEK_SET);
  if (cur_offset != start_offset) {
    PLOG(ERROR) << "Failed to seek to offset: " << start_offset;
    Cleanup(ErrorCode::kFilesystemVerifierError);
    return;
  }
  const auto read_size =
      std::min<size_t>(buffer_size, end_offset - start_offset);
  const auto bytes_read = fd->Read(buffer, read_size);
  if (bytes_read < 0 || static_cast<size_t>(bytes_read) != read_size) {
    PLOG(ERROR) << "Failed to read offset " << start_offset << " expected "
                << read_size << " bytes, actual: " << bytes_read;
    Cleanup(ErrorCode::kFilesystemVerifierError);
    return;
  }
  if (!hasher_->Update(buffer, read_size)) {
    LOG(ERROR) << "Hasher updated failed on offset" << start_offset;
    Cleanup(ErrorCode::kFilesystemVerifierError);
    return;
  }
  const auto progress = (start_offset + bytes_read) * 1.0f / partition_size_;
  UpdatePartitionProgress(progress * (1 - kVerityProgressPercent) +
                          kVerityProgressPercent);
  CHECK(pending_task_id_.PostTask(
      FROM_HERE,
      base::BindOnce(&FilesystemVerifierAction::HashPartition,
                     base::Unretained(this),
                     fd,
                     start_offset + bytes_read,
                     end_offset,
                     buffer,
                     buffer_size)));
}

void FilesystemVerifierAction::StartPartitionHashing() {
  if (partition_index_ == install_plan_.partitions.size()) {
    if (!install_plan_.untouched_dynamic_partitions.empty()) {
      LOG(INFO) << "Verifying extents of untouched dynamic partitions ["
                << base::JoinString(install_plan_.untouched_dynamic_partitions,
                                    ", ")
                << "]";
      if (!dynamic_control_->VerifyExtentsForUntouchedPartitions(
              install_plan_.source_slot,
              install_plan_.target_slot,
              install_plan_.untouched_dynamic_partitions)) {
        Cleanup(ErrorCode::kFilesystemVerifierError);
        return;
      }
    }

    Cleanup(ErrorCode::kSuccess);
    return;
  }
  const InstallPlan::Partition& partition =
      install_plan_.partitions[partition_index_];
  const auto& part_path = GetPartitionPath();
  partition_size_ = GetPartitionSize();

  LOG(INFO) << "Hashing partition " << partition_index_ << " ("
            << partition.name << ") on device " << part_path;
  auto success = false;
  if (IsVABC(partition)) {
    success = InitializeFdVABC(ShouldWriteVerity());
  } else {
    if (part_path.empty()) {
      if (partition_size_ == 0) {
        LOG(INFO) << "Skip hashing partition " << partition_index_ << " ("
                  << partition.name << ") because size is 0.";
        partition_index_++;
        StartPartitionHashing();
        return;
      }
      LOG(ERROR) << "Cannot hash partition " << partition_index_ << " ("
                 << partition.name
                 << ") because its device path cannot be determined.";
      Cleanup(ErrorCode::kFilesystemVerifierError);
      return;
    }
    success = InitializeFd(part_path);
  }
  if (!success) {
    Cleanup(ErrorCode::kFilesystemVerifierError);
    return;
  }
  buffer_.resize(kReadFileBufferSize);
  hasher_ = std::make_unique<HashCalculator>();

  offset_ = 0;
  filesystem_data_end_ = partition_size_;
  CHECK_LE(partition.hash_tree_offset, partition.fec_offset)
      << " Hash tree is expected to come before FEC data";
  if (partition.hash_tree_offset != 0) {
    filesystem_data_end_ = partition.hash_tree_offset;
  } else if (partition.fec_offset != 0) {
    filesystem_data_end_ = partition.fec_offset;
  }
  if (ShouldWriteVerity()) {
    LOG(INFO) << "Verity writes enabled on partition " << partition.name;
    if (!verity_writer_->Init(partition)) {
      LOG(INFO) << "Verity writes enabled on partition " << partition.name;
      Cleanup(ErrorCode::kVerityCalculationError);
      return;
    }
    WriteVerityAndHashPartition(
        partition_fd_, 0, filesystem_data_end_, buffer_.data(), buffer_.size());
  } else {
    LOG(INFO) << "Verity writes disabled on partition " << partition.name;
    HashPartition(
        partition_fd_, 0, partition_size_, buffer_.data(), buffer_.size());
  }
}

bool FilesystemVerifierAction::IsVABC(
    const InstallPlan::Partition& partition) const {
  return dynamic_control_->UpdateUsesSnapshotCompression() &&
         verifier_step_ == VerifierStep::kVerifyTargetHash &&
         dynamic_control_->IsDynamicPartition(partition.name,
                                              install_plan_.target_slot);
}

const std::string& FilesystemVerifierAction::GetPartitionPath() const {
  const InstallPlan::Partition& partition =
      install_plan_.partitions[partition_index_];
  switch (verifier_step_) {
    case VerifierStep::kVerifySourceHash:
      return partition.source_path;
    case VerifierStep::kVerifyTargetHash:
      if (IsVABC(partition)) {
        return partition.readonly_target_path;
      } else {
        return partition.target_path;
      }
  }
}

size_t FilesystemVerifierAction::GetPartitionSize() const {
  const InstallPlan::Partition& partition =
      install_plan_.partitions[partition_index_];
  switch (verifier_step_) {
    case VerifierStep::kVerifySourceHash:
      return partition.source_size;
    case VerifierStep::kVerifyTargetHash:
      return partition.target_size;
  }
}

bool FilesystemVerifierAction::ShouldWriteVerity() {
  const InstallPlan::Partition& partition =
      install_plan_.partitions[partition_index_];
  return verifier_step_ == VerifierStep::kVerifyTargetHash &&
         install_plan_.write_verity &&
         (partition.hash_tree_size > 0 || partition.fec_size > 0);
}

void FilesystemVerifierAction::FinishPartitionHashing() {
  if (!hasher_->Finalize()) {
    LOG(ERROR) << "Unable to finalize the hash.";
    Cleanup(ErrorCode::kError);
    return;
  }
  InstallPlan::Partition& partition =
      install_plan_.partitions[partition_index_];
  LOG(INFO) << "Hash of " << partition.name << ": "
            << Base64Encode(hasher_->raw_hash());

  switch (verifier_step_) {
    case VerifierStep::kVerifyTargetHash:
      if (partition.target_hash != hasher_->raw_hash()) {
        LOG(ERROR) << "New '" << partition.name
                   << "' partition verification failed.";
        if (partition.source_hash.empty()) {
          // No need to verify source if it is a full payload.
          Cleanup(ErrorCode::kNewRootfsVerificationError);
          return;
        }
        // If we have not verified source partition yet, now that the target
        // partition does not match, and it's not a full payload, we need to
        // switch to kVerifySourceHash step to check if it's because the
        // source partition does not match either.
        verifier_step_ = VerifierStep::kVerifySourceHash;
      } else {
        partition_index_++;
      }
      break;
    case VerifierStep::kVerifySourceHash:
      if (partition.source_hash != hasher_->raw_hash()) {
        LOG(ERROR) << "Old '" << partition.name
                   << "' partition verification failed.";
        LOG(ERROR) << "This is a server-side error due to mismatched delta"
                   << " update image!";
        LOG(ERROR) << "The delta I've been given contains a " << partition.name
                   << " delta update that must be applied over a "
                   << partition.name << " with a specific checksum, but the "
                   << partition.name
                   << " we're starting with doesn't have that checksum! This"
                      " means that the delta I've been given doesn't match my"
                      " existing system. The "
                   << partition.name << " partition I have has hash: "
                   << Base64Encode(hasher_->raw_hash())
                   << " but the update expected me to have "
                   << Base64Encode(partition.source_hash) << " .";
        LOG(INFO) << "To get the checksum of the " << partition.name
                  << " partition run this command: dd if="
                  << partition.source_path
                  << " bs=1M count=" << partition.source_size
                  << " iflag=count_bytes 2>/dev/null | openssl dgst -sha256 "
                     "-binary | openssl base64";
        LOG(INFO) << "To get the checksum of partitions in a bin file, "
                  << "run: .../src/scripts/sha256_partitions.sh .../file.bin";
        Cleanup(ErrorCode::kDownloadStateInitializationError);
        return;
      }
      // The action will skip kVerifySourceHash step if target partition hash
      // matches, if we are in this step, it means target hash does not match,
      // and now that the source partition hash matches, we should set the
      // error code to reflect the error in target partition. We only need to
      // verify the source partition which the target hash does not match, the
      // rest of the partitions don't matter.
      Cleanup(ErrorCode::kNewRootfsVerificationError);
      return;
  }
  // Start hashing the next partition, if any.
  hasher_.reset();
  buffer_.clear();
  if (partition_fd_) {
    partition_fd_->Close();
    partition_fd_.reset();
  }
  StartPartitionHashing();
}

}  // namespace chromeos_update_engine
