// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/mtd_file_descriptor.h"

#include <fcntl.h>
#include <mtd/ubi-user.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/files/file_path.h>
#include <base/strings/string_number_conversions.h>

#include "update_engine/utils.h"

namespace {

static const char kSysfsClassUbi[] = "/sys/class/ubi/";
static const char kUsableEbSize[] = "/usable_eb_size";
static const char kReservedEbs[] = "/reserved_ebs";

using chromeos_update_engine::UbiVolumeInfo;
using chromeos_update_engine::utils::ReadFile;

// Return a UbiVolumeInfo pointer if |path| is a UBI volume. Otherwise, return
// a null unique pointer.
std::unique_ptr<UbiVolumeInfo> GetUbiVolumeInfo(const char* path) {
  base::FilePath device_node(path);
  base::FilePath ubi_name(device_node.BaseName());

  std::string sysfs_node(kSysfsClassUbi);
  sysfs_node.append(ubi_name.MaybeAsASCII());

  std::unique_ptr<UbiVolumeInfo> ret;

  // Obtain volume info from sysfs.
  std::string s_reserved_ebs;
  if (!ReadFile(sysfs_node + kReservedEbs, &s_reserved_ebs)) {
    return ret;
  }
  std::string s_eb_size;
  if (!ReadFile(sysfs_node + kUsableEbSize, &s_eb_size)) {
    return ret;
  }

  size_t reserved_ebs, eb_size;
  if (!base::StringToSizeT(s_reserved_ebs, &reserved_ebs)) {
    return ret;
  }
  if (!base::StringToSizeT(s_eb_size, &eb_size)) {
    return ret;
  }

  ret.reset(new UbiVolumeInfo);
  ret->size = reserved_ebs * eb_size;
  return ret;
}

}  // namespace

namespace chromeos_update_engine {

MtdFileDescriptor::MtdFileDescriptor()
    : read_ctx_(nullptr, &mtd_read_close),
      write_ctx_(nullptr, &mtd_write_close) {}

bool MtdFileDescriptor::IsMtd(const char* path) {
  uint64_t size;
  return mtd_node_info(path, &size, nullptr, nullptr) == 0;
}

bool MtdFileDescriptor::Open(const char* path, int flags, mode_t mode) {
  // This File Descriptor does not support read and write.
  TEST_AND_RETURN_FALSE((flags & O_RDWR) != O_RDWR);
  TEST_AND_RETURN_FALSE(
      EintrSafeFileDescriptor::Open(path, flags | O_CLOEXEC, mode));

  if (flags & O_RDONLY) {
    read_ctx_.reset(mtd_read_descriptor(fd_, path));
  } else if (flags & O_WRONLY) {
    write_ctx_.reset(mtd_write_descriptor(fd_, path));
  }

  if (!read_ctx_ && !write_ctx_) {
    Close();
    return false;
  }

  return true;
}

bool MtdFileDescriptor::Open(const char* path, int flags) {
  mode_t cur = umask(022);
  umask(cur);
  return Open(path, flags, 0777 & ~cur);
}

ssize_t MtdFileDescriptor::Read(void* buf, size_t count) {
  CHECK(read_ctx_);
  return mtd_read_data(read_ctx_.get(), static_cast<char*>(buf), count);
}

ssize_t MtdFileDescriptor::Write(const void* buf, size_t count) {
  CHECK(write_ctx_);
  return mtd_write_data(write_ctx_.get(), static_cast<const char*>(buf), count);
}

off64_t MtdFileDescriptor::Seek(off64_t offset, int whence) {
  CHECK(read_ctx_);
  return EintrSafeFileDescriptor::Seek(offset, whence);
}

void MtdFileDescriptor::Reset() {
  EintrSafeFileDescriptor::Reset();
  read_ctx_.reset();
  write_ctx_.reset();
}

bool UbiFileDescriptor::IsUbi(const char* path) {
  return static_cast<bool>(GetUbiVolumeInfo(path));
}

std::unique_ptr<UbiVolumeInfo> UbiFileDescriptor::CreateWriteContext(
    const char* path) {
  std::unique_ptr<UbiVolumeInfo> info = GetUbiVolumeInfo(path);
  uint64_t volume_size;
  if (info && (ioctl(fd_, UBI_IOCVOLUP, &volume_size) != 0 ||
               volume_size != info->size)) {
    info.reset();
  }
  return info;
}

bool UbiFileDescriptor::Open(const char* path, int flags, mode_t mode) {
  // This File Descriptor does not support read and write.
  TEST_AND_RETURN_FALSE((flags & O_RDWR) != O_RDWR);
  TEST_AND_RETURN_FALSE(
      EintrSafeFileDescriptor::Open(path, flags | O_CLOEXEC, mode));

  if (flags & O_RDONLY) {
    read_ctx_ = GetUbiVolumeInfo(path);
  } else if (flags & O_WRONLY) {
    write_ctx_ = CreateWriteContext(path);
  }

  if (!read_ctx_ && !write_ctx_) {
    Close();
    return false;
  }

  return true;
}

bool UbiFileDescriptor::Open(const char* path, int flags) {
  mode_t cur = umask(022);
  umask(cur);
  return Open(path, flags, 0777 & ~cur);
}

ssize_t UbiFileDescriptor::Read(void* buf, size_t count) {
  CHECK(read_ctx_);
  return EintrSafeFileDescriptor::Read(buf, count);
}

ssize_t UbiFileDescriptor::Write(const void* buf, size_t count) {
  CHECK(write_ctx_);
  return EintrSafeFileDescriptor::Write(buf, count);
}

off64_t UbiFileDescriptor::Seek(off64_t offset, int whence) {
  CHECK(read_ctx_);
  return EintrSafeFileDescriptor::Seek(offset, whence);
}

void UbiFileDescriptor::Reset() {
  EintrSafeFileDescriptor::Reset();
  read_ctx_.reset();
  write_ctx_.reset();
}

}  // namespace chromeos_update_engine
