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

#include <cstdint>
#include <memory>

#include <libsnapshot/snapshot_writer.h>

#include "update_engine/payload_consumer/file_descriptor.h"

namespace chromeos_update_engine {

// A Readable/Writable FileDescriptor class. This is a simple wrapper around
// CowWriter. Only intended to be used by FileSystemVerifierAction for writing
// FEC. Writes must be block aligned(4096) or write will fail.
class CowWriterFileDescriptor final : public FileDescriptor {
 public:
  explicit CowWriterFileDescriptor(
      std::unique_ptr<android::snapshot::ISnapshotWriter> cow_writer);
  ~CowWriterFileDescriptor();

  bool Open(const char* path, int flags, mode_t mode) override;
  bool Open(const char* path, int flags) override;

  ssize_t Read(void* buf, size_t count) override;

  // |count| must be block aligned, current offset of this fd must also be block
  // aligned.
  ssize_t Write(const void* buf, size_t count) override;

  off64_t Seek(off64_t offset, int whence) override;

  uint64_t BlockDevSize() override;

  bool BlkIoctl(int request,
                uint64_t start,
                uint64_t length,
                int* result) override;

  bool Flush() override;

  bool Close() override;

  bool IsSettingErrno() override;

  bool IsOpen() override;

 private:
  std::unique_ptr<android::snapshot::ISnapshotWriter> cow_writer_;
  FileDescriptorPtr cow_reader_;
  bool dirty_ = false;
};
}  // namespace chromeos_update_engine
