// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/filesystem_verifier_action.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <cstdlib>
#include <string>

#include <base/bind.h>
#include <base/posix/eintr_wrapper.h>

#include "update_engine/hardware_interface.h"
#include "update_engine/system_state.h"
#include "update_engine/utils.h"

using chromeos::MessageLoop;
using std::string;

namespace chromeos_update_engine {

namespace {
const off_t kReadFileBufferSize = 128 * 1024;
}  // namespace

FilesystemVerifierAction::FilesystemVerifierAction(
    SystemState* system_state,
    PartitionType partition_type)
    : partition_type_(partition_type),
      remaining_size_(kint64max),
      system_state_(system_state) {}

void FilesystemVerifierAction::PerformAction() {
  // Will tell the ActionProcessor we've failed if we return.
  ScopedActionCompleter abort_action_completer(processor_, this);

  if (!HasInputObject()) {
    LOG(ERROR) << "FilesystemVerifierAction missing input object.";
    return;
  }
  install_plan_ = GetInputObject();

  if (partition_type_ == PartitionType::kKernel) {
    LOG(INFO) << "verifying kernel, marking as unbootable";
    if (!system_state_->hardware()->MarkKernelUnbootable(
        install_plan_.kernel_install_path)) {
      PLOG(ERROR) << "Unable to clear kernel GPT boot flags: " <<
          install_plan_.kernel_install_path;
    }
  }

  if (install_plan_.is_full_update &&
      (partition_type_ == PartitionType::kSourceRootfs ||
       partition_type_ == PartitionType::kSourceKernel)) {
    // No hash verification needed. Done!
    LOG(INFO) << "filesystem verifying skipped on full update.";
    if (HasOutputPipe())
      SetOutputObject(install_plan_);
    abort_action_completer.set_code(ErrorCode::kSuccess);
    return;
  }

  string target_path;
  switch (partition_type_) {
    case PartitionType::kRootfs:
      target_path = install_plan_.install_path;
      if (target_path.empty()) {
        utils::GetInstallDev(system_state_->hardware()->BootDevice(),
                             &target_path);
      }
      break;
    case PartitionType::kKernel:
      target_path = install_plan_.kernel_install_path;
      if (target_path.empty()) {
        string rootfs_path;
        utils::GetInstallDev(system_state_->hardware()->BootDevice(),
                             &rootfs_path);
        target_path = utils::KernelDeviceOfBootDevice(rootfs_path);
      }
      break;
    case PartitionType::kSourceRootfs:
      target_path = install_plan_.source_path.empty() ?
                    system_state_->hardware()->BootDevice() :
                    install_plan_.source_path;
      break;
    case PartitionType::kSourceKernel:
      target_path = install_plan_.kernel_source_path.empty() ?
                    utils::KernelDeviceOfBootDevice(
                        system_state_->hardware()->BootDevice()) :
                    install_plan_.kernel_source_path;
      break;
  }

  src_fd_ = HANDLE_EINTR(open(target_path.c_str(), O_RDONLY));
  if (src_fd_ < 0) {
    PLOG(ERROR) << "Unable to open " << target_path << " for reading";
    return;
  }

  DetermineFilesystemSize(src_fd_);
  buffer_.resize(kReadFileBufferSize);

  // Start the first read.
  read_task_ = MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE,
      src_fd_,
      MessageLoop::WatchMode::kWatchRead,
      true,  // persistent
      base::Bind(&FilesystemVerifierAction::OnReadReadyCallback,
                 base::Unretained(this)));

  abort_action_completer.set_should_complete(false);
}

void FilesystemVerifierAction::TerminateProcessing() {
  cancelled_ = true;
  Cleanup(ErrorCode::kSuccess);  // error code is ignored if canceled_ is true.
}

bool FilesystemVerifierAction::IsCleanupPending() const {
  return (src_fd_ != -1);
}

void FilesystemVerifierAction::Cleanup(ErrorCode code) {
  MessageLoop::current()->CancelTask(read_task_);
  read_task_ = MessageLoop::kTaskIdNull;

  if (src_fd_ != -1) {
    if (IGNORE_EINTR(close(src_fd_)) != 0) {
      PLOG(ERROR) << "Error closing fd " << src_fd_;
    }
    src_fd_ = -1;
  }
  // This memory is not used anymore.
  buffer_.clear();

  if (cancelled_)
    return;
  if (code == ErrorCode::kSuccess && HasOutputPipe())
    SetOutputObject(install_plan_);
  processor_->ActionComplete(this, code);
}

void FilesystemVerifierAction::OnReadReadyCallback() {
  size_t bytes_to_read = std::min(static_cast<int64_t>(buffer_.size()),
                                  remaining_size_);

  ssize_t bytes_read = 0;
  if (bytes_to_read) {
    bytes_read = HANDLE_EINTR(read(src_fd_, buffer_.data(), bytes_to_read));
  }
  if (bytes_read < 0) {
    PLOG(ERROR) << "Read failed";
    failed_ = true;
  } else if (bytes_read == 0) {
    read_done_ = true;
  } else {
    remaining_size_ -= bytes_read;
  }

  if (bytes_read > 0) {
    CHECK(!read_done_);
    if (!hasher_.Update(buffer_.data(), bytes_read)) {
      LOG(ERROR) << "Unable to update the hash.";
      failed_ = true;
    }
  }

  CheckTerminationConditions();
}

void FilesystemVerifierAction::CheckTerminationConditions() {
  if (failed_ || cancelled_) {
    Cleanup(ErrorCode::kError);
    return;
  }

  if (read_done_) {
    // We're done!
    ErrorCode code = ErrorCode::kSuccess;
    if (hasher_.Finalize()) {
      LOG(INFO) << "Hash: " << hasher_.hash();
      switch (partition_type_) {
        case PartitionType::kRootfs:
          if (install_plan_.rootfs_hash != hasher_.raw_hash()) {
            code = ErrorCode::kNewRootfsVerificationError;
            LOG(ERROR) << "New rootfs verification failed.";
          }
          break;
        case PartitionType::kKernel:
          if (install_plan_.kernel_hash != hasher_.raw_hash()) {
            code = ErrorCode::kNewKernelVerificationError;
            LOG(ERROR) << "New kernel verification failed.";
          }
          break;
        case PartitionType::kSourceRootfs:
          install_plan_.source_rootfs_hash = hasher_.raw_hash();
          break;
        case PartitionType::kSourceKernel:
          install_plan_.source_kernel_hash = hasher_.raw_hash();
          break;
      }
    } else {
      LOG(ERROR) << "Unable to finalize the hash.";
      code = ErrorCode::kError;
    }
    Cleanup(code);
  }
}

void FilesystemVerifierAction::DetermineFilesystemSize(int fd) {
  switch (partition_type_) {
    case PartitionType::kRootfs:
      remaining_size_ = install_plan_.rootfs_size;
      LOG(INFO) << "Filesystem size: " << remaining_size_ << " bytes.";
      break;
    case PartitionType::kKernel:
      remaining_size_ = install_plan_.kernel_size;
      LOG(INFO) << "Filesystem size: " << remaining_size_ << " bytes.";
      break;
    case PartitionType::kSourceRootfs:
      {
        int block_count = 0, block_size = 0;
        if (utils::GetFilesystemSizeFromFD(fd, &block_count, &block_size)) {
          remaining_size_ = static_cast<int64_t>(block_count) * block_size;
          LOG(INFO) << "Filesystem size: " << remaining_size_ << " bytes ("
                    << block_count << "x" << block_size << ").";
        }
      }
      break;
    default:
      break;
  }
  return;
}

}  // namespace chromeos_update_engine
