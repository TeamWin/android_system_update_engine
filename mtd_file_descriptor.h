// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_MTD_FILE_DESCRIPTOR_H_
#define UPDATE_ENGINE_MTD_FILE_DESCRIPTOR_H_

// This module defines file descriptors that deal with NAND media. We are
// concerned with raw NAND access (as MTD device), and through UBI layer.

#include <mtdutils.h>

#include "update_engine/file_descriptor.h"

namespace chromeos_update_engine {

// A class defining the file descriptor API for raw MTD device. This file
// descriptor supports either random read, or sequential write but not both at
// once.
class MtdFileDescriptor : public EintrSafeFileDescriptor {
 public:
  MtdFileDescriptor();

  static bool IsMtd(const char* path);

  bool Open(const char* path, int flags, mode_t mode) override;
  bool Open(const char* path, int flags) override;
  ssize_t Read(void* buf, size_t count) override;
  ssize_t Write(const void* buf, size_t count) override;
  off64_t Seek(off64_t offset, int whence) override;
  bool Close() override;

 private:
  std::unique_ptr<MtdReadContext, decltype(&mtd_read_close)> read_ctx_;
  std::unique_ptr<MtdWriteContext, decltype(&mtd_write_close)> write_ctx_;
  uint64_t nr_written_;
};

struct UbiVolumeInfo {
  // Number of eraseblocks.
  uint64_t reserved_ebs;
  // Size of each eraseblock.
  uint64_t eraseblock_size;
};

// A file descriptor to update a UBI volume, similar to MtdFileDescriptor.
// Once the file descriptor is opened for write, the volume is marked as being
// updated. The volume will not be usable until an update is completed. See
// UBI_IOCVOLUP ioctl operation.
class UbiFileDescriptor : public EintrSafeFileDescriptor {
 public:
  // Perform some queries about |path| to see if it is a UBI volume.
  static bool IsUbi(const char* path);

  bool Open(const char* path, int flags, mode_t mode) override;
  bool Open(const char* path, int flags) override;
  ssize_t Read(void* buf, size_t count) override;
  ssize_t Write(const void* buf, size_t count) override;
  off64_t Seek(off64_t offset, int whence) override;
  bool Close() override;

 private:
  enum Mode {
    kReadOnly,
    kWriteOnly
  };

  uint64_t usable_eb_blocks_;
  uint64_t eraseblock_size_;
  uint64_t volume_size_;
  uint64_t nr_written_;

  Mode mode_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_MTD_FILE_DESCRIPTOR_H_
