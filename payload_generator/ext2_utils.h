// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_EXT2_UTILS_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_EXT2_UTILS_H_

#include <ext2fs/ext2fs.h>

// Utility class to close a file system.
class ScopedExt2fsCloser {
 public:
  explicit ScopedExt2fsCloser(ext2_filsys filsys) : filsys_(filsys) {}
  ~ScopedExt2fsCloser() { ext2fs_close(filsys_); }

 private:
  ext2_filsys filsys_;
  DISALLOW_COPY_AND_ASSIGN(ScopedExt2fsCloser);
};

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_EXT2_UTILS_H_
