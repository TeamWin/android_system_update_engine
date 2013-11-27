// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/download_action.h"
#include <errno.h>
#include <algorithm>
#include <string>
#include <vector>
#include <glib.h>

#include <base/file_path.h>
#include <base/stringprintf.h>

#include "update_engine/action_pipe.h"
#include "update_engine/p2p_manager.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

using std::min;
using std::string;
using std::vector;
using base::FilePath;
using base::StringPrintf;

namespace chromeos_update_engine {

DownloadAction::DownloadAction(PrefsInterface* prefs,
                               SystemState* system_state,
                               HttpFetcher* http_fetcher)
    : prefs_(prefs),
      system_state_(system_state),
      http_fetcher_(http_fetcher),
      writer_(NULL),
      code_(kErrorCodeSuccess),
      delegate_(NULL),
      bytes_received_(0),
      p2p_sharing_fd_(-1),
      p2p_visible_(true) {}

DownloadAction::~DownloadAction() {}

void DownloadAction::CloseP2PSharingFd(bool delete_p2p_file) {
  if (p2p_sharing_fd_ != -1) {
    if (close(p2p_sharing_fd_) != 0) {
      PLOG(ERROR) << "Error closing p2p sharing fd";
    }
    p2p_sharing_fd_ = -1;
  }

  if (delete_p2p_file) {
    FilePath path = system_state_->p2p_manager()->FileGetPath(p2p_file_id_);
    if (unlink(path.value().c_str()) != 0) {
      PLOG(ERROR) << "Error deleting p2p file " << path.value();
    } else {
      LOG(INFO) << "Deleted p2p file " << path.value();
    }
  }

  // Don't use p2p from this point onwards.
  p2p_file_id_.clear();
}

bool DownloadAction::SetupP2PSharingFd() {
  P2PManager *p2p_manager = system_state_->p2p_manager();

  if (!p2p_manager->FileShare(p2p_file_id_, install_plan_.payload_size)) {
    LOG(ERROR) << "Unable to share file via p2p";
    CloseP2PSharingFd(true); // delete p2p file
    return false;
  }

  // File has already been created (and allocated, xattrs been
  // populated etc.) by FileShare() so just open it for writing.
  FilePath path = p2p_manager->FileGetPath(p2p_file_id_);
  p2p_sharing_fd_ = open(path.value().c_str(), O_WRONLY);
  if (p2p_sharing_fd_ == -1) {
    PLOG(ERROR) << "Error opening file " << path.value();
    CloseP2PSharingFd(true); // Delete p2p file.
    return false;
  }

  // Ensure file to share is world-readable, otherwise
  // p2p-server and p2p-http-server can't access it.
  //
  // (Q: Why doesn't the file have mode 0644 already? A: Because
  // the process-wide umask is set to 0700 in main.cc.)
  if (fchmod(p2p_sharing_fd_, 0644) != 0) {
    PLOG(ERROR) << "Error setting mode 0644 on " << path.value();
    CloseP2PSharingFd(true); // Delete p2p file.
    return false;
  }

  // All good.
  LOG(INFO) << "Writing payload contents to " << path.value();
  p2p_manager->FileGetVisible(p2p_file_id_, &p2p_visible_);
  return true;
}

void DownloadAction::WriteToP2PFile(const char *data,
                                    size_t length,
                                    off_t file_offset) {
  if (p2p_sharing_fd_ == -1) {
    if (!SetupP2PSharingFd())
      return;
  }

  // Check that the file is at least |file_offset| bytes long - if
  // it's not something is wrong and we must immediately delete the
  // file to avoid propagating this problem to other peers.
  //
  // How can this happen? It could be that we're resuming an update
  // after a system crash... in this case, it could be that
  //
  //  1. the p2p file didn't get properly synced to stable storage; or
  //  2. the file was deleted at bootup (it's in /var/cache after all); or
  //  3. other reasons
  struct stat statbuf;
  if (fstat(p2p_sharing_fd_, &statbuf) != 0) {
    PLOG(ERROR) << "Error getting file status for p2p file";
    CloseP2PSharingFd(true); // Delete p2p file.
    return;
  }
  if (statbuf.st_size < file_offset) {
    LOG(ERROR) << "Wanting to write to file offset " << file_offset
               << " but existing p2p file is only " << statbuf.st_size
               << " bytes.";
    CloseP2PSharingFd(true); // Delete p2p file.
    return;
  }

  off_t cur_file_offset = lseek(p2p_sharing_fd_, file_offset, SEEK_SET);
  if (cur_file_offset != static_cast<off_t>(file_offset)) {
    PLOG(ERROR) << "Error seeking to position "
                << file_offset << " in p2p file";
    CloseP2PSharingFd(true); // Delete p2p file.
  } else {
    // OK, seeking worked, now write the data
    ssize_t bytes_written = write(p2p_sharing_fd_, data, length);
    if (bytes_written != static_cast<ssize_t>(length)) {
      PLOG(ERROR) << "Error writing "
                  << length << " bytes at file offset "
                  << file_offset << " in p2p file";
      CloseP2PSharingFd(true); // Delete p2p file.
    }
  }
}

void DownloadAction::PerformAction() {
  http_fetcher_->set_delegate(this);

  // Get the InstallPlan and read it
  CHECK(HasInputObject());
  install_plan_ = GetInputObject();
  bytes_received_ = 0;

  install_plan_.Dump();

  if (writer_) {
    LOG(INFO) << "Using writer for test.";
  } else {
    delta_performer_.reset(new DeltaPerformer(prefs_,
                                              system_state_,
                                              &install_plan_));
    writer_ = delta_performer_.get();
  }
  int rc = writer_->Open(install_plan_.install_path.c_str(),
                         O_TRUNC | O_WRONLY | O_CREAT | O_LARGEFILE,
                         0644);
  if (rc < 0) {
    LOG(ERROR) << "Unable to open output file " << install_plan_.install_path;
    // report error to processor
    processor_->ActionComplete(this, kErrorCodeInstallDeviceOpenError);
    return;
  }
  if (delta_performer_.get() &&
      !delta_performer_->OpenKernel(
          install_plan_.kernel_install_path.c_str())) {
    LOG(ERROR) << "Unable to open kernel file "
               << install_plan_.kernel_install_path.c_str();
    writer_->Close();
    processor_->ActionComplete(this, kErrorCodeKernelDeviceOpenError);
    return;
  }
  if (delegate_) {
    delegate_->SetDownloadStatus(true);  // Set to active.
  }

  if (system_state_ != NULL) {
    string file_id = utils::CalculateP2PFileId(install_plan_.payload_hash,
                                               install_plan_.payload_size);
    if (system_state_->request_params()->use_p2p_for_sharing()) {
      // If we're sharing the update, store the file_id to convey
      // that we should write to the file.
      p2p_file_id_ = file_id;
      LOG(INFO) << "p2p file id: " << p2p_file_id_;
    } else {
      // Even if we're not sharing the update, it could be that
      // there's a partial file from a previous attempt with the same
      // hash. If this is the case, we NEED to clean it up otherwise
      // we're essentially timing out other peers downloading from us
      // (since we're never going to complete the file).
      FilePath path = system_state_->p2p_manager()->FileGetPath(file_id);
      if (!path.empty()) {
        if (unlink(path.value().c_str()) != 0) {
          PLOG(ERROR) << "Error deleting p2p file " << path.value();
        } else {
          LOG(INFO) << "Deleting partial p2p file " << path.value()
                    << " since we're not using p2p to share.";
        }
      }
    }
  }

  // Tweak timeouts on the HTTP fetcher if we're downloading from a
  // local peer.
  if (system_state_ != NULL &&
      system_state_->request_params()->use_p2p_for_downloading() &&
      system_state_->request_params()->p2p_url() ==
      install_plan_.download_url) {
    LOG(INFO) << "Tweaking HTTP fetcher since we're downloading via p2p";
    http_fetcher_->set_low_speed_limit(kDownloadP2PLowSpeedLimitBps,
                                       kDownloadP2PLowSpeedTimeSeconds);
    http_fetcher_->set_max_retry_count(kDownloadP2PMaxRetryCount);
    http_fetcher_->set_connect_timeout(kDownloadP2PConnectTimeoutSeconds);
  }

  http_fetcher_->BeginTransfer(install_plan_.download_url);
}

void DownloadAction::TerminateProcessing() {
  if (writer_) {
    writer_->Close();
    writer_ = NULL;
  }
  if (delegate_) {
    delegate_->SetDownloadStatus(false);  // Set to inactive.
  }
  CloseP2PSharingFd(false); // Keep p2p file.
  // Terminates the transfer. The action is terminated, if necessary, when the
  // TransferTerminated callback is received.
  http_fetcher_->TerminateTransfer();
}

void DownloadAction::SeekToOffset(off_t offset) {
  bytes_received_ = offset;
}

void DownloadAction::ReceivedBytes(HttpFetcher *fetcher,
                                   const char* bytes,
                                   int length) {
  // Note that bytes_received_ is the current offset.
  if (!p2p_file_id_.empty()) {
    WriteToP2PFile(bytes, length, bytes_received_);
  }

  bytes_received_ += length;
  if (delegate_)
    delegate_->BytesReceived(bytes_received_, install_plan_.payload_size);
  if (writer_ && !writer_->Write(bytes, length, &code_)) {
    LOG(ERROR) << "Error " << code_ << " in DeltaPerformer's Write method when "
               << "processing the received payload -- Terminating processing";
    // Delete p2p file, if applicable.
    if (!p2p_file_id_.empty())
      CloseP2PSharingFd(true);
    // Don't tell the action processor that the action is complete until we get
    // the TransferTerminated callback. Otherwise, this and the HTTP fetcher
    // objects may get destroyed before all callbacks are complete.
    TerminateProcessing();
    return;
  }

  // Call p2p_manager_->FileMakeVisible() when we've successfully
  // verified the manifest!
  if (!p2p_visible_ &&
      delta_performer_.get() && delta_performer_->IsManifestValid()) {
    LOG(INFO) << "Manifest has been validated. Making p2p file visible.";
    system_state_->p2p_manager()->FileMakeVisible(p2p_file_id_);
    p2p_visible_ = true;
  }
}

void DownloadAction::TransferComplete(HttpFetcher *fetcher, bool successful) {
  if (writer_) {
    LOG_IF(WARNING, writer_->Close() != 0) << "Error closing the writer.";
    writer_ = NULL;
  }
  if (delegate_) {
    delegate_->SetDownloadStatus(false);  // Set to inactive.
  }
  ErrorCode code =
      successful ? kErrorCodeSuccess : kErrorCodeDownloadTransferError;
  if (code == kErrorCodeSuccess && delta_performer_.get()) {
    code = delta_performer_->VerifyPayload(install_plan_.payload_hash,
                                           install_plan_.payload_size);
    if (code != kErrorCodeSuccess) {
      LOG(ERROR) << "Download of " << install_plan_.download_url
                 << " failed due to payload verification error.";
      // Delete p2p file, if applicable.
      if (!p2p_file_id_.empty())
        CloseP2PSharingFd(true);
    } else if (!delta_performer_->GetNewPartitionInfo(
        &install_plan_.kernel_size,
        &install_plan_.kernel_hash,
        &install_plan_.rootfs_size,
        &install_plan_.rootfs_hash)) {
      LOG(ERROR) << "Unable to get new partition hash info.";
      code = kErrorCodeDownloadNewPartitionInfoError;
    }
  }

  // Write the path to the output pipe if we're successful.
  if (code == kErrorCodeSuccess && HasOutputPipe())
    SetOutputObject(install_plan_);
  processor_->ActionComplete(this, code);
}

void DownloadAction::TransferTerminated(HttpFetcher *fetcher) {
  if (code_ != kErrorCodeSuccess) {
    processor_->ActionComplete(this, code_);
  }
}

};  // namespace {}
