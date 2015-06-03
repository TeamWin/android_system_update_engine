// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/extent_mapper.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>

#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/extent_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace extent_mapper {

namespace {
const int kBlockSize = 4096;
}

bool ExtentsForFileChunkFibmap(const string& path,
                               off_t chunk_offset,
                               off_t chunk_size,
                               vector<Extent>* out) {
  CHECK(out);
  CHECK_EQ(0, chunk_offset % kBlockSize);
  CHECK(chunk_size == -1 || chunk_size >= 0);
  struct stat stbuf;
  int rc = stat(path.c_str(), &stbuf);
  TEST_AND_RETURN_FALSE_ERRNO(rc == 0);
  TEST_AND_RETURN_FALSE(S_ISREG(stbuf.st_mode));

  int fd = open(path.c_str(), O_RDONLY, 0);
  TEST_AND_RETURN_FALSE_ERRNO(fd >= 0);
  ScopedFdCloser fd_closer(&fd);

  // Get file size in blocks
  off_t file_size = utils::FileSize(fd);
  if (file_size < 0) {
    return false;
  }
  CHECK_LE(chunk_offset, file_size);
  off_t size = file_size - chunk_offset;
  if (chunk_size != -1) {
    size = std::min(size, chunk_size);
  }
  const int block_count = (size + kBlockSize - 1) / kBlockSize;
  const int start_block = chunk_offset / kBlockSize;
  Extent current;
  current.set_start_block(0);
  current.set_num_blocks(0);

  for (int i = start_block; i < start_block + block_count; i++) {
    unsigned int block32 = i;
    rc = ioctl(fd, FIBMAP, &block32);
    TEST_AND_RETURN_FALSE_ERRNO(rc == 0);

    const uint64_t block = (block32 == 0 ? kSparseHole : block32);

    AppendBlockToExtents(out, block);
  }
  return true;
}

bool ExtentsForFileFibmap(const string& path, vector<Extent>* out) {
  return ExtentsForFileChunkFibmap(path, 0, -1, out);
}

bool GetFilesystemBlockSize(const string& path, uint32_t* out_blocksize) {
  int fd = open(path.c_str(), O_RDONLY, 0);
  TEST_AND_RETURN_FALSE_ERRNO(fd >= 0);
  ScopedFdCloser fd_closer(&fd);
  int rc = ioctl(fd, FIGETBSZ, out_blocksize);
  TEST_AND_RETURN_FALSE_ERRNO(rc != -1);
  return true;
}

}  // namespace extent_mapper

}  // namespace chromeos_update_engine
