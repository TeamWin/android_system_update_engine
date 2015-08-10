// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/blob_file_writer.h"

#include "update_engine/utils.h"

namespace chromeos_update_engine {

off_t BlobFileWriter::StoreBlob(const chromeos::Blob& blob) {
  base::AutoLock auto_lock(blob_mutex_);
  if (!utils::PWriteAll(blob_fd_, blob.data(), blob.size(), *blob_file_size_))
    return -1;

  off_t result = *blob_file_size_;
  *blob_file_size_ += blob.size();

  stored_blobs_++;
  if (total_blobs_ > 0 &&
      (10 * (stored_blobs_ - 1) / total_blobs_) !=
      (10 * stored_blobs_ / total_blobs_)) {
    LOG(INFO) << (100 * stored_blobs_ / total_blobs_)
              << "% complete " << stored_blobs_ << "/" << total_blobs_
              << " ops (output size: " << *blob_file_size_ << ")";
  }
  return result;
}

void BlobFileWriter::SetTotalBlobs(size_t total_blobs) {
  total_blobs_ = total_blobs;
}

}  // namespace chromeos_update_engine
