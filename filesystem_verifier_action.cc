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
#include <chromeos/streams/file_stream.h>

#include "update_engine/hardware_interface.h"
#include "update_engine/system_state.h"
#include "update_engine/utils.h"

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

  chromeos::ErrorPtr error;
  src_stream_ = chromeos::FileStream::Open(
      base::FilePath(target_path),
      chromeos::Stream::AccessMode::READ,
      chromeos::FileStream::Disposition::OPEN_EXISTING,
      &error);

  if (!src_stream_) {
    LOG(ERROR) << "Unable to open " << target_path << " for reading";
    return;
  }

  DetermineFilesystemSize(target_path);
  buffer_.resize(kReadFileBufferSize);

  // Start the first read.
  ScheduleRead();

  abort_action_completer.set_should_complete(false);
}

void FilesystemVerifierAction::TerminateProcessing() {
  cancelled_ = true;
  Cleanup(ErrorCode::kSuccess);  // error code is ignored if canceled_ is true.
}

bool FilesystemVerifierAction::IsCleanupPending() const {
  return src_stream_ != nullptr;
}

void FilesystemVerifierAction::Cleanup(ErrorCode code) {
  src_stream_.reset();
  // This memory is not used anymore.
  buffer_.clear();

  if (cancelled_)
    return;
  if (code == ErrorCode::kSuccess && HasOutputPipe())
    SetOutputObject(install_plan_);
  processor_->ActionComplete(this, code);
}

void FilesystemVerifierAction::ScheduleRead() {
  size_t bytes_to_read = std::min(static_cast<int64_t>(buffer_.size()),
                                  remaining_size_);
  if (!bytes_to_read) {
    OnReadDoneCallback(0);
    return;
  }

  bool read_async_ok = src_stream_->ReadAsync(
    buffer_.data(),
    bytes_to_read,
    base::Bind(&FilesystemVerifierAction::OnReadDoneCallback,
               base::Unretained(this)),
    base::Bind(&FilesystemVerifierAction::OnReadErrorCallback,
               base::Unretained(this)),
    nullptr);

  if (!read_async_ok) {
    LOG(ERROR) << "Unable to schedule an asynchronous read from the stream.";
    Cleanup(ErrorCode::kError);
  }
}

void FilesystemVerifierAction::OnReadDoneCallback(size_t bytes_read) {
  if (bytes_read == 0) {
    read_done_ = true;
  } else {
    remaining_size_ -= bytes_read;
    CHECK(!read_done_);
    if (!hasher_.Update(buffer_.data(), bytes_read)) {
      LOG(ERROR) << "Unable to update the hash.";
      Cleanup(ErrorCode::kError);
      return;
    }
  }

  // We either terminate the action or have more data to read.
  if (!CheckTerminationConditions())
    ScheduleRead();
}

void FilesystemVerifierAction::OnReadErrorCallback(
      const chromeos::Error* error) {
  // TODO(deymo): Transform the read-error into an specific ErrorCode.
  LOG(ERROR) << "Asynchronous read failed.";
  Cleanup(ErrorCode::kError);
}

bool FilesystemVerifierAction::CheckTerminationConditions() {
  if (cancelled_) {
    Cleanup(ErrorCode::kError);
    return true;
  }

  if (!read_done_)
    return false;

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
  return true;
}

void FilesystemVerifierAction::DetermineFilesystemSize(
    const std::string& path) {
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
        if (utils::GetFilesystemSize(path, &block_count, &block_size)) {
          remaining_size_ = static_cast<int64_t>(block_count) * block_size;
          LOG(INFO) << "Filesystem size: " << remaining_size_ << " bytes ("
                    << block_count << "x" << block_size << ").";
        }
      }
      break;
    default:
      break;
  }
}

}  // namespace chromeos_update_engine
