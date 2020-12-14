//
// Copyright (C) 2011 The Android Open Source Project
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

#include "update_engine/common/download_action.h"

#include <errno.h>

#include <algorithm>
#include <string>

#include <base/files/file_path.h>
#include <base/metrics/statistics_recorder.h>
#include <base/strings/stringprintf.h>

#include "update_engine/common/action_pipe.h"
#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/error_code_utils.h"
#include "update_engine/common/multi_range_http_fetcher.h"
#include "update_engine/common/prefs_interface.h"
#include "update_engine/common/utils.h"

using base::FilePath;
using std::string;

namespace chromeos_update_engine {

DownloadAction::DownloadAction(PrefsInterface* prefs,
                               BootControlInterface* boot_control,
                               HardwareInterface* hardware,
                               HttpFetcher* http_fetcher,
                               bool interactive)
    : prefs_(prefs),
      boot_control_(boot_control),
      hardware_(hardware),
      http_fetcher_(new MultiRangeHttpFetcher(http_fetcher)),
      interactive_(interactive),
      code_(ErrorCode::kSuccess),
      delegate_(nullptr) {}

DownloadAction::~DownloadAction() {}

void DownloadAction::PerformAction() {
  http_fetcher_->set_delegate(this);

  // Get the InstallPlan and read it
  CHECK(HasInputObject());
  install_plan_ = GetInputObject();
  install_plan_.Dump();

  bytes_received_ = 0;
  bytes_received_previous_payloads_ = 0;
  bytes_total_ = 0;
  for (const auto& payload : install_plan_.payloads)
    bytes_total_ += payload.size;

  if (install_plan_.is_resume) {
    int64_t payload_index = 0;
    if (prefs_->GetInt64(kPrefsUpdateStatePayloadIndex, &payload_index) &&
        static_cast<size_t>(payload_index) < install_plan_.payloads.size()) {
      // Save the index for the resume payload before downloading any previous
      // payload, otherwise it will be overwritten.
      resume_payload_index_ = payload_index;
      for (int i = 0; i < payload_index; i++)
        install_plan_.payloads[i].already_applied = true;
    }
  }
  CHECK_GE(install_plan_.payloads.size(), 1UL);
  if (!payload_)
    payload_ = &install_plan_.payloads[0];

  LOG(INFO) << "Marking new slot as unbootable";
  if (!boot_control_->MarkSlotUnbootable(install_plan_.target_slot)) {
    LOG(WARNING) << "Unable to mark new slot "
                 << BootControlInterface::SlotName(install_plan_.target_slot)
                 << ". Proceeding with the update anyway.";
  }

  StartDownloading();
}

bool DownloadAction::LoadCachedManifest(int64_t manifest_size) {
  std::string cached_manifest_bytes;
  if (!prefs_->GetString(kPrefsManifestBytes, &cached_manifest_bytes) ||
      cached_manifest_bytes.size() <= 0) {
    LOG(INFO) << "Cached Manifest data not found";
    return false;
  }
  if (static_cast<int64_t>(cached_manifest_bytes.size()) != manifest_size) {
    LOG(WARNING) << "Cached metadata has unexpected size: "
                 << cached_manifest_bytes.size() << " vs. " << manifest_size;
    return false;
  }

  ErrorCode error;
  const bool success =
      delta_performer_->Write(
          cached_manifest_bytes.data(), cached_manifest_bytes.size(), &error) &&
      delta_performer_->IsManifestValid();
  if (success) {
    LOG(INFO) << "Successfully parsed cached manifest";
  } else {
    // If parsing of cached data failed, fall back to fetch them using HTTP
    LOG(WARNING) << "Cached manifest data fails to load, error code:"
                 << static_cast<int>(error) << "," << error;
  }
  return success;
}

void DownloadAction::StartDownloading() {
  download_active_ = true;
  http_fetcher_->ClearRanges();

  if (delta_performer_ != nullptr) {
    LOG(INFO) << "Using writer for test.";
  } else {
    delta_performer_.reset(new DeltaPerformer(prefs_,
                                              boot_control_,
                                              hardware_,
                                              delegate_,
                                              &install_plan_,
                                              payload_,
                                              interactive_));
  }

  if (install_plan_.is_resume &&
      payload_ == &install_plan_.payloads[resume_payload_index_]) {
    // Resuming an update so parse the cached manifest first
    int64_t manifest_metadata_size = 0;
    int64_t manifest_signature_size = 0;
    prefs_->GetInt64(kPrefsManifestMetadataSize, &manifest_metadata_size);
    prefs_->GetInt64(kPrefsManifestSignatureSize, &manifest_signature_size);

    // TODO(zhangkelvin) Add unittest for success and fallback route
    if (!LoadCachedManifest(manifest_metadata_size + manifest_signature_size)) {
      if (delta_performer_) {
        // Create a new DeltaPerformer to reset all its state
        delta_performer_ = std::make_unique<DeltaPerformer>(prefs_,
                                                            boot_control_,
                                                            hardware_,
                                                            delegate_,
                                                            &install_plan_,
                                                            payload_,
                                                            interactive_);
      }
      http_fetcher_->AddRange(base_offset_,
                              manifest_metadata_size + manifest_signature_size);
    }

    // If there're remaining unprocessed data blobs, fetch them. Be careful
    // not to request data beyond the end of the payload to avoid 416 HTTP
    // response error codes.
    int64_t next_data_offset = 0;
    prefs_->GetInt64(kPrefsUpdateStateNextDataOffset, &next_data_offset);
    uint64_t resume_offset =
        manifest_metadata_size + manifest_signature_size + next_data_offset;
    if (!payload_->size) {
      http_fetcher_->AddRange(base_offset_ + resume_offset);
    } else if (resume_offset < payload_->size) {
      http_fetcher_->AddRange(base_offset_ + resume_offset,
                              payload_->size - resume_offset);
    }
  } else {
    if (payload_->size) {
      http_fetcher_->AddRange(base_offset_, payload_->size);
    } else {
      // If no payload size is passed we assume we read until the end of the
      // stream.
      http_fetcher_->AddRange(base_offset_);
    }
  }

  http_fetcher_->BeginTransfer(install_plan_.download_url);
}

void DownloadAction::SuspendAction() {
  http_fetcher_->Pause();
}

void DownloadAction::ResumeAction() {
  http_fetcher_->Unpause();
}

void DownloadAction::TerminateProcessing() {
  if (delta_performer_) {
    delta_performer_->Close();
    delta_performer_.reset();
  }
  download_active_ = false;
  // Terminates the transfer. The action is terminated, if necessary, when the
  // TransferTerminated callback is received.
  http_fetcher_->TerminateTransfer();
}

void DownloadAction::SeekToOffset(off_t offset) {
  bytes_received_ = offset;
}

bool DownloadAction::ReceivedBytes(HttpFetcher* fetcher,
                                   const void* bytes,
                                   size_t length) {
  bytes_received_ += length;
  uint64_t bytes_downloaded_total =
      bytes_received_previous_payloads_ + bytes_received_;
  if (delegate_ && download_active_) {
    delegate_->BytesReceived(length, bytes_downloaded_total, bytes_total_);
  }
  if (delta_performer_ && !delta_performer_->Write(bytes, length, &code_)) {
    if (code_ != ErrorCode::kSuccess) {
      LOG(ERROR) << "Error " << utils::ErrorCodeToString(code_) << " (" << code_
                 << ") in DeltaPerformer's Write method when "
                 << "processing the received payload -- Terminating processing";
    }
    // Don't tell the action processor that the action is complete until we get
    // the TransferTerminated callback. Otherwise, this and the HTTP fetcher
    // objects may get destroyed before all callbacks are complete.
    TerminateProcessing();
    return false;
  }

  return true;
}

void DownloadAction::TransferComplete(HttpFetcher* fetcher, bool successful) {
  if (delta_performer_) {
    LOG_IF(WARNING, delta_performer_->Close() != 0)
        << "Error closing the writer.";
  }
  download_active_ = false;
  ErrorCode code =
      successful ? ErrorCode::kSuccess : ErrorCode::kDownloadTransferError;
  if (code == ErrorCode::kSuccess) {
    if (delta_performer_ && !payload_->already_applied)
      code = delta_performer_->VerifyPayload(payload_->hash, payload_->size);
    if (code == ErrorCode::kSuccess) {
      CHECK_EQ(install_plan_.payloads.size(), 1UL);
      // All payloads have been applied and verified.
      if (delegate_)
        delegate_->DownloadComplete();

      // Log UpdateEngine.DownloadAction.* histograms to help diagnose
      // long-blocking operations.
      std::string histogram_output;
      base::StatisticsRecorder::WriteGraph("UpdateEngine.DownloadAction.",
                                           &histogram_output);
      LOG(INFO) << histogram_output;
    } else {
      LOG(ERROR) << "Download of " << install_plan_.download_url
                 << " failed due to payload verification error.";
    }
  }

  // Write the path to the output pipe if we're successful.
  if (code == ErrorCode::kSuccess && HasOutputPipe())
    SetOutputObject(install_plan_);
  processor_->ActionComplete(this, code);
}

void DownloadAction::TransferTerminated(HttpFetcher* fetcher) {
  if (code_ != ErrorCode::kSuccess) {
    processor_->ActionComplete(this, code_);
  } else if (payload_->already_applied) {
    LOG(INFO) << "TransferTerminated with ErrorCode::kSuccess when the current "
                 "payload has already applied, treating as TransferComplete.";
    TransferComplete(fetcher, true);
  }
}

}  // namespace chromeos_update_engine
