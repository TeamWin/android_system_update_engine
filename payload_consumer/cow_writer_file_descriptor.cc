//
// Copyright (C) 2021 The Android Open Source Project
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

#include "update_engine/payload_consumer/cow_writer_file_descriptor.h"

#include <memory>
#include <utility>

#include <base/logging.h>

#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/file_descriptor.h"

namespace chromeos_update_engine {
CowWriterFileDescriptor::CowWriterFileDescriptor(
    std::unique_ptr<android::snapshot::ISnapshotWriter> cow_writer)
    : cow_writer_(std::move(cow_writer)),
      cow_reader_(cow_writer_->OpenReader()) {}

bool CowWriterFileDescriptor::Open(const char* path, int flags, mode_t mode) {
  LOG(ERROR) << "CowWriterFileDescriptor doesn't support Open()";
  return false;
}
bool CowWriterFileDescriptor::Open(const char* path, int flags) {
  LOG(ERROR) << "CowWriterFileDescriptor doesn't support Open()";
  return false;
}

ssize_t CowWriterFileDescriptor::Read(void* buf, size_t count) {
  if (dirty_) {
    // OK, CowReader provides a snapshot view of what the cow contains. Which
    // means any writes happened after opening a CowReader isn't visible to
    // that CowReader. Therefore, we re-open CowReader whenever we attempt a
    // read after write. This does incur an overhead everytime you read after
    // write.
    // The usage of |dirty_| flag to coordinate re-open is a very coarse grained
    // checked. This implementation has suboptimal performance. For better
    // performance, keep track of blocks which are overwritten, and only re-open
    // if reading a dirty block.
    // TODO(b/173432386) Implement finer grained dirty checks
    const auto offset = cow_reader_->Seek(0, SEEK_CUR);
    cow_reader_.reset();
    if (!cow_writer_->Finalize()) {
      LOG(ERROR) << "Failed to Finalize() cow writer";
      return -1;
    }
    cow_reader_ = cow_writer_->OpenReader();
    if (cow_reader_ == nullptr) {
      LOG(ERROR) << "Failed to re-open cow reader after writing to COW";
      return -1;
    }
    const auto pos = cow_reader_->Seek(offset, SEEK_SET);
    if (pos != offset) {
      LOG(ERROR) << "Failed to seek to previous position after re-opening cow "
                    "reader, expected "
                 << offset << " actual: " << pos;
      return -1;
    }
    dirty_ = false;
  }
  return cow_reader_->Read(buf, count);
}

ssize_t CowWriterFileDescriptor::Write(const void* buf, size_t count) {
  auto offset = cow_reader_->Seek(0, SEEK_CUR);
  CHECK_EQ(offset % cow_writer_->options().block_size, 0);
  auto success = cow_writer_->AddRawBlocks(
      offset / cow_writer_->options().block_size, buf, count);
  if (success) {
    if (cow_reader_->Seek(count, SEEK_CUR) < 0) {
      return -1;
    }
    dirty_ = true;
    return count;
  }
  return -1;
}

off64_t CowWriterFileDescriptor::Seek(const off64_t offset, int whence) {
  return cow_reader_->Seek(offset, whence);
}

uint64_t CowWriterFileDescriptor::BlockDevSize() {
  LOG(ERROR) << "CowWriterFileDescriptor doesn't support BlockDevSize()";
  return 0;
}

bool CowWriterFileDescriptor::BlkIoctl(int request,
                                       uint64_t start,
                                       uint64_t length,
                                       int* result) {
  LOG(ERROR) << "CowWriterFileDescriptor doesn't support BlkIoctl()";
  return false;
}

bool CowWriterFileDescriptor::Flush() {
  // CowWriter already automatilly flushes, no need to do anything.
  return true;
}

bool CowWriterFileDescriptor::Close() {
  if (cow_writer_) {
    TEST_AND_RETURN_FALSE(cow_writer_->Finalize());
    cow_writer_ = nullptr;
  }
  if (cow_reader_) {
    TEST_AND_RETURN_FALSE(cow_reader_->Close());
    cow_reader_ = nullptr;
  }
  return true;
}

bool CowWriterFileDescriptor::IsSettingErrno() {
  return false;
}

bool CowWriterFileDescriptor::IsOpen() {
  return cow_writer_ != nullptr && cow_reader_ != nullptr;
}

CowWriterFileDescriptor::~CowWriterFileDescriptor() {
  Close();
}

}  // namespace chromeos_update_engine
