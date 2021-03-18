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
#include <brillo/streams/file_stream.h>

#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/file_descriptor.h"

using brillo::data_encoding::Base64Encode;
using std::string;

namespace chromeos_update_engine {

namespace {
const off_t kReadFileBufferSize = 128 * 1024;
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
  brillo::MessageLoop::current()->CancelTask(pending_task_id_);
  cancelled_ = true;
  Cleanup(ErrorCode::kSuccess);  // error code is ignored if canceled_ is true.
}

void FilesystemVerifierAction::Cleanup(ErrorCode code) {
  read_fd_.reset();
  write_fd_.reset();
  // This memory is not used anymore.
  buffer_.clear();

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

bool FilesystemVerifierAction::InitializeFdVABC() {
  const InstallPlan::Partition& partition =
      install_plan_.partitions[partition_index_];

  read_fd_ = dynamic_control_->OpenCowReader(
      partition.name, partition.source_path, true);
  if (!read_fd_) {
    LOG(ERROR) << "OpenCowReader(" << partition.name << ", "
               << partition.source_path << ") failed.";
    return false;
  }
  partition_size_ = partition.target_size;
  // TODO(b/173432386): Support Verity writes for VABC.
  CHECK_EQ(partition.fec_size, 0U);
  CHECK_EQ(partition.hash_tree_size, 0U);
  return true;
}

bool FilesystemVerifierAction::InitializeFd(const std::string& part_path) {
  read_fd_ = FileDescriptorPtr(new EintrSafeFileDescriptor());
  if (!read_fd_->Open(part_path.c_str(), O_RDONLY)) {
    LOG(ERROR) << "Unable to open " << part_path << " for reading.";
    return false;
  }

  // Can't re-use |read_fd_|, as verity writer may call `seek` to modify state
  // of a file descriptor.
  if (ShouldWriteVerity()) {
    write_fd_ = FileDescriptorPtr(new EintrSafeFileDescriptor());
    if (!write_fd_->Open(part_path.c_str(), O_RDWR)) {
      LOG(ERROR) << "Unable to open " << part_path << " for Read/Write.";
      return false;
    }
  }
  return true;
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
  string part_path;
  switch (verifier_step_) {
    case VerifierStep::kVerifySourceHash:
      part_path = partition.source_path;
      partition_size_ = partition.source_size;
      break;
    case VerifierStep::kVerifyTargetHash:
      part_path = partition.target_path;
      partition_size_ = partition.target_size;
      break;
  }

  LOG(INFO) << "Hashing partition " << partition_index_ << " ("
            << partition.name << ") on device " << part_path;
  auto success = false;
  if (dynamic_control_->UpdateUsesSnapshotCompression() &&
      verifier_step_ == VerifierStep::kVerifyTargetHash &&
      dynamic_control_->IsDynamicPartition(partition.name,
                                           install_plan_.target_slot)) {
    success = InitializeFdVABC();
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
  if (ShouldWriteVerity()) {
    if (!verity_writer_->Init(partition, read_fd_, write_fd_)) {
      Cleanup(ErrorCode::kVerityCalculationError);
      return;
    }
  }

  // Start the first read.
  ScheduleRead();
}

bool FilesystemVerifierAction::ShouldWriteVerity() {
  const InstallPlan::Partition& partition =
      install_plan_.partitions[partition_index_];
  return verifier_step_ == VerifierStep::kVerifyTargetHash &&
         install_plan_.write_verity &&
         (partition.hash_tree_size > 0 || partition.fec_size > 0);
}

void FilesystemVerifierAction::ScheduleRead() {
  const InstallPlan::Partition& partition =
      install_plan_.partitions[partition_index_];

  // We can only start reading anything past |hash_tree_offset| after we have
  // already read all the data blocks that the hash tree covers. The same
  // applies to FEC.
  uint64_t read_end = partition_size_;
  if (partition.hash_tree_size != 0 &&
      offset_ < partition.hash_tree_data_offset + partition.hash_tree_data_size)
    read_end = std::min(read_end, partition.hash_tree_offset);
  if (partition.fec_size != 0 &&
      offset_ < partition.fec_data_offset + partition.fec_data_size)
    read_end = std::min(read_end, partition.fec_offset);
  size_t bytes_to_read =
      std::min(static_cast<uint64_t>(buffer_.size()), read_end - offset_);
  if (!bytes_to_read) {
    FinishPartitionHashing();
    return;
  }

  auto bytes_read = read_fd_->Read(buffer_.data(), bytes_to_read);
  if (bytes_read < 0) {
    LOG(ERROR) << "Unable to schedule an asynchronous read from the stream.";
    Cleanup(ErrorCode::kError);
  } else {
    // We could just invoke |OnReadDoneCallback()|, it works. But |PostTask|
    // is used so that users can cancel updates.
    pending_task_id_ = brillo::MessageLoop::current()->PostTask(
        base::Bind(&FilesystemVerifierAction::OnReadDone,
                   base::Unretained(this),
                   bytes_read));
  }
}

void FilesystemVerifierAction::OnReadDone(size_t bytes_read) {
  if (cancelled_) {
    Cleanup(ErrorCode::kError);
    return;
  }
  if (bytes_read == 0) {
    LOG(ERROR) << "Failed to read the remaining " << partition_size_ - offset_
               << " bytes from partition "
               << install_plan_.partitions[partition_index_].name;
    Cleanup(ErrorCode::kFilesystemVerifierError);
    return;
  }

  if (!hasher_->Update(buffer_.data(), bytes_read)) {
    LOG(ERROR) << "Unable to update the hash.";
    Cleanup(ErrorCode::kError);
    return;
  }

  // WE don't consider sizes of each partition. Every partition
  // has the same length on progress bar.
  // TODO(zhangkelvin) Take sizes of each partition into account

  UpdateProgress(
      (static_cast<double>(offset_) / partition_size_ + partition_index_) /
      install_plan_.partitions.size());
  if (ShouldWriteVerity()) {
    if (!verity_writer_->Update(offset_, buffer_.data(), bytes_read)) {
      Cleanup(ErrorCode::kVerityCalculationError);
      return;
    }
  }

  offset_ += bytes_read;

  if (offset_ == partition_size_) {
    FinishPartitionHashing();
    return;
  }

  ScheduleRead();
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
  if (read_fd_) {
    read_fd_.reset();
  }
  if (write_fd_) {
    write_fd_.reset();
  }
  StartPartitionHashing();
}

}  // namespace chromeos_update_engine
