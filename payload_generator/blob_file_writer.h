// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_BLOB_FILE_WRITER_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_BLOB_FILE_WRITER_H_

#include <base/macros.h>

#include <base/synchronization/lock.h>
#include <chromeos/secure_blob.h>

namespace chromeos_update_engine {

class BlobFileWriter {
 public:
  // Create the BlobFileWriter object that will manage the blobs stored to
  // |blob_fd| in a thread safe way.
  BlobFileWriter(int blob_fd, off_t* blob_file_size)
    : blob_fd_(blob_fd),
      blob_file_size_(blob_file_size) {}

  // Store the passed |blob| in the blob file. Returns the offset at which it
  // was stored, or -1 in case of failure.
  off_t StoreBlob(const chromeos::Blob& blob);

  // The number of |total_blobs| is the number of blobs that will be stored but
  // is only used for logging purposes. If not set, logging will be skipped.
  void SetTotalBlobs(size_t total_blobs);

 private:
  size_t total_blobs_{0};
  size_t stored_blobs_{0};

  // The file and its size are protected with the |blob_mutex_|.
  int blob_fd_;
  off_t* blob_file_size_;

  base::Lock blob_mutex_;

  DISALLOW_COPY_AND_ASSIGN(BlobFileWriter);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_BLOB_FILE_WRITER_H_
