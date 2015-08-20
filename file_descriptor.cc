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

#include "update_engine/file_descriptor.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/posix/eintr_wrapper.h>

namespace chromeos_update_engine {

bool EintrSafeFileDescriptor::Open(const char* path, int flags, mode_t mode) {
  CHECK_EQ(fd_, -1);
  return ((fd_ = HANDLE_EINTR(open(path, flags, mode))) >= 0);
}

bool EintrSafeFileDescriptor::Open(const char* path, int flags) {
  CHECK_EQ(fd_, -1);
  return ((fd_ = HANDLE_EINTR(open(path, flags))) >= 0);
}

ssize_t EintrSafeFileDescriptor::Read(void* buf, size_t count) {
  CHECK_GE(fd_, 0);
  return HANDLE_EINTR(read(fd_, buf, count));
}

ssize_t EintrSafeFileDescriptor::Write(const void* buf, size_t count) {
  CHECK_GE(fd_, 0);

  // Attempt repeated writes, as long as some progress is being made.
  char* char_buf = const_cast<char*>(reinterpret_cast<const char*>(buf));
  ssize_t written = 0;
  while (count > 0) {
    ssize_t ret = HANDLE_EINTR(write(fd_, char_buf, count));

    // Fail on either an error or no progress.
    if (ret <= 0)
      return (written ? written : ret);
    written += ret;
    count -= ret;
    char_buf += ret;
  }
  return written;
}

off64_t EintrSafeFileDescriptor::Seek(off64_t offset, int whence) {
  CHECK_GE(fd_, 0);
  return lseek64(fd_, offset, whence);
}

bool EintrSafeFileDescriptor::Close() {
  CHECK_GE(fd_, 0);
  if (IGNORE_EINTR(close(fd_)))
    return false;
  Reset();
  return true;
}

void EintrSafeFileDescriptor::Reset() {
  fd_ = -1;
}

}  // namespace chromeos_update_engine
