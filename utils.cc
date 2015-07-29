// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/utils.h"

#include <stdint.h>

#include <dirent.h>
#include <elf.h>
#include <errno.h>
#include <ext2fs/ext2fs.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <utility>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/data_encoding.h>
#include <chromeos/message_loops/message_loop.h>

#include "update_engine/clock_interface.h"
#include "update_engine/constants.h"
#include "update_engine/file_descriptor.h"
#include "update_engine/file_writer.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/subprocess.h"
#include "update_engine/system_state.h"
#include "update_engine/update_attempter.h"

using base::Time;
using base::TimeDelta;
using std::min;
using std::pair;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

// The following constants control how UnmountFilesystem should retry if
// umount() fails with an errno EBUSY, i.e. retry 5 times over the course of
// one second.
const int kUnmountMaxNumOfRetries = 5;
const int kUnmountRetryIntervalInMicroseconds = 200 * 1000;  // 200 ms

// Number of bytes to read from a file to attempt to detect its contents. Used
// in GetFileFormat.
const int kGetFileFormatMaxHeaderSize = 32;

// Return true if |disk_name| is an MTD or a UBI device. Note that this test is
// simply based on the name of the device.
bool IsMtdDeviceName(const string& disk_name) {
  return base::StartsWithASCII(disk_name, "/dev/ubi", true) ||
         base::StartsWithASCII(disk_name, "/dev/mtd", true);
}

// Return the device name for the corresponding partition on a NAND device.
// WARNING: This function returns device names that are not mountable.
string MakeNandPartitionName(int partition_num) {
  switch (partition_num) {
    case 2:
    case 4:
    case 6: {
      return base::StringPrintf("/dev/mtd%d", partition_num);
    }
    default: {
      return base::StringPrintf("/dev/ubi%d_0", partition_num);
    }
  }
}

// Return the device name for the corresponding partition on a NAND device that
// may be mountable (but may not be writable).
string MakeNandPartitionNameForMount(int partition_num) {
  switch (partition_num) {
    case 2:
    case 4:
    case 6: {
      return base::StringPrintf("/dev/mtd%d", partition_num);
    }
    case 3:
    case 5:
    case 7: {
      return base::StringPrintf("/dev/ubiblock%d_0", partition_num);
    }
    default: {
      return base::StringPrintf("/dev/ubi%d_0", partition_num);
    }
  }
}

}  // namespace

namespace utils {

// Cgroup container is created in update-engine's upstart script located at
// /etc/init/update-engine.conf.
static const char kCGroupDir[] = "/sys/fs/cgroup/cpu/update-engine";

string ParseECVersion(string input_line) {
  base::TrimWhitespaceASCII(input_line, base::TRIM_ALL, &input_line);

  // At this point we want to convert the format key=value pair from mosys to
  // a vector of key value pairs.
  vector<pair<string, string>> kv_pairs;
  if (base::SplitStringIntoKeyValuePairs(input_line, '=', ' ', &kv_pairs)) {
    for (const pair<string, string>& kv_pair : kv_pairs) {
      // Finally match against the fw_verion which may have quotes.
      if (kv_pair.first == "fw_version") {
        string output;
        // Trim any quotes.
        base::TrimString(kv_pair.second, "\"", &output);
        return output;
      }
    }
  }
  LOG(ERROR) << "Unable to parse fwid from ec info.";
  return "";
}


const string KernelDeviceOfBootDevice(const string& boot_device) {
  string kernel_partition_name;

  string disk_name;
  int partition_num;
  if (SplitPartitionName(boot_device, &disk_name, &partition_num)) {
    // Currently this assumes the partition number of the boot device is
    // 3, 5, or 7, and changes it to 2, 4, or 6, respectively, to
    // get the kernel device.
    if (partition_num == 3 || partition_num == 5 || partition_num == 7) {
      kernel_partition_name = MakePartitionName(disk_name, partition_num - 1);
    }
  }

  return kernel_partition_name;
}


bool WriteFile(const char* path, const void* data, int data_len) {
  DirectFileWriter writer;
  TEST_AND_RETURN_FALSE_ERRNO(0 == writer.Open(path,
                                               O_WRONLY | O_CREAT | O_TRUNC,
                                               0600));
  ScopedFileWriterCloser closer(&writer);
  TEST_AND_RETURN_FALSE_ERRNO(writer.Write(data, data_len));
  return true;
}

bool WriteAll(int fd, const void* buf, size_t count) {
  const char* c_buf = static_cast<const char*>(buf);
  ssize_t bytes_written = 0;
  while (bytes_written < static_cast<ssize_t>(count)) {
    ssize_t rc = write(fd, c_buf + bytes_written, count - bytes_written);
    TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
    bytes_written += rc;
  }
  return true;
}

bool PWriteAll(int fd, const void* buf, size_t count, off_t offset) {
  const char* c_buf = static_cast<const char*>(buf);
  size_t bytes_written = 0;
  int num_attempts = 0;
  while (bytes_written < count) {
    num_attempts++;
    ssize_t rc = pwrite(fd, c_buf + bytes_written, count - bytes_written,
                        offset + bytes_written);
    // TODO(garnold) for debugging failure in chromium-os:31077; to be removed.
    if (rc < 0) {
      PLOG(ERROR) << "pwrite error; num_attempts=" << num_attempts
                  << " bytes_written=" << bytes_written
                  << " count=" << count << " offset=" << offset;
    }
    TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
    bytes_written += rc;
  }
  return true;
}

bool WriteAll(FileDescriptorPtr fd, const void* buf, size_t count) {
  const char* c_buf = static_cast<const char*>(buf);
  ssize_t bytes_written = 0;
  while (bytes_written < static_cast<ssize_t>(count)) {
    ssize_t rc = fd->Write(c_buf + bytes_written, count - bytes_written);
    TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
    bytes_written += rc;
  }
  return true;
}

bool PWriteAll(FileDescriptorPtr fd,
               const void* buf,
               size_t count,
               off_t offset) {
  TEST_AND_RETURN_FALSE_ERRNO(fd->Seek(offset, SEEK_SET) !=
                              static_cast<off_t>(-1));
  return WriteAll(fd, buf, count);
}

bool PReadAll(int fd, void* buf, size_t count, off_t offset,
              ssize_t* out_bytes_read) {
  char* c_buf = static_cast<char*>(buf);
  ssize_t bytes_read = 0;
  while (bytes_read < static_cast<ssize_t>(count)) {
    ssize_t rc = pread(fd, c_buf + bytes_read, count - bytes_read,
                       offset + bytes_read);
    TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
    if (rc == 0) {
      break;
    }
    bytes_read += rc;
  }
  *out_bytes_read = bytes_read;
  return true;
}

bool PReadAll(FileDescriptorPtr fd, void* buf, size_t count, off_t offset,
              ssize_t* out_bytes_read) {
  TEST_AND_RETURN_FALSE_ERRNO(fd->Seek(offset, SEEK_SET) !=
                              static_cast<off_t>(-1));
  char* c_buf = static_cast<char*>(buf);
  ssize_t bytes_read = 0;
  while (bytes_read < static_cast<ssize_t>(count)) {
    ssize_t rc = fd->Read(c_buf + bytes_read, count - bytes_read);
    TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
    if (rc == 0) {
      break;
    }
    bytes_read += rc;
  }
  *out_bytes_read = bytes_read;
  return true;
}

// Append |nbytes| of content from |buf| to the vector pointed to by either
// |vec_p| or |str_p|.
static void AppendBytes(const uint8_t* buf, size_t nbytes,
                        chromeos::Blob* vec_p) {
  CHECK(buf);
  CHECK(vec_p);
  vec_p->insert(vec_p->end(), buf, buf + nbytes);
}
static void AppendBytes(const uint8_t* buf, size_t nbytes,
                        string* str_p) {
  CHECK(buf);
  CHECK(str_p);
  str_p->append(buf, buf + nbytes);
}

// Reads from an open file |fp|, appending the read content to the container
// pointer to by |out_p|.  Returns true upon successful reading all of the
// file's content, false otherwise. If |size| is not -1, reads up to |size|
// bytes.
template <class T>
static bool Read(FILE* fp, off_t size, T* out_p) {
  CHECK(fp);
  CHECK(size == -1 || size >= 0);
  uint8_t buf[1024];
  while (size == -1 || size > 0) {
    off_t bytes_to_read = sizeof(buf);
    if (size > 0 && bytes_to_read > size) {
      bytes_to_read = size;
    }
    size_t nbytes = fread(buf, 1, bytes_to_read, fp);
    if (!nbytes) {
      break;
    }
    AppendBytes(buf, nbytes, out_p);
    if (size != -1) {
      CHECK(size >= static_cast<off_t>(nbytes));
      size -= nbytes;
    }
  }
  if (ferror(fp)) {
    return false;
  }
  return size == 0 || feof(fp);
}

// Opens a file |path| for reading and appends its the contents to a container
// |out_p|. Starts reading the file from |offset|. If |offset| is beyond the end
// of the file, returns success. If |size| is not -1, reads up to |size| bytes.
template <class T>
static bool ReadFileChunkAndAppend(const string& path,
                                   off_t offset,
                                   off_t size,
                                   T* out_p) {
  CHECK_GE(offset, 0);
  CHECK(size == -1 || size >= 0);
  base::ScopedFILE fp(fopen(path.c_str(), "r"));
  if (!fp.get())
    return false;
  if (offset) {
    // Return success without appending any data if a chunk beyond the end of
    // the file is requested.
    if (offset >= FileSize(path)) {
      return true;
    }
    TEST_AND_RETURN_FALSE_ERRNO(fseek(fp.get(), offset, SEEK_SET) == 0);
  }
  return Read(fp.get(), size, out_p);
}

// TODO(deymo): This is only used in unittest, but requires the private
// Read<string>() defined here. Expose Read<string>() or move to base/ version.
bool ReadPipe(const string& cmd, string* out_p) {
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return false;
  bool success = Read(fp, -1, out_p);
  return (success && pclose(fp) >= 0);
}

bool ReadFile(const string& path, chromeos::Blob* out_p) {
  return ReadFileChunkAndAppend(path, 0, -1, out_p);
}

bool ReadFile(const string& path, string* out_p) {
  return ReadFileChunkAndAppend(path, 0, -1, out_p);
}

bool ReadFileChunk(const string& path, off_t offset, off_t size,
                   chromeos::Blob* out_p) {
  return ReadFileChunkAndAppend(path, offset, size, out_p);
}

off_t BlockDevSize(int fd) {
  uint64_t dev_size;
  int rc = ioctl(fd, BLKGETSIZE64, &dev_size);
  if (rc == -1) {
    dev_size = -1;
    PLOG(ERROR) << "Error running ioctl(BLKGETSIZE64) on " << fd;
  }
  return dev_size;
}

off_t BlockDevSize(const string& path) {
  int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    PLOG(ERROR) << "Error opening " << path;
    return fd;
  }

  off_t dev_size = BlockDevSize(fd);
  if (dev_size == -1)
    PLOG(ERROR) << "Error getting block device size on " << path;

  close(fd);
  return dev_size;
}

off_t FileSize(int fd) {
  struct stat stbuf;
  int rc = fstat(fd, &stbuf);
  CHECK_EQ(rc, 0);
  if (rc < 0) {
    PLOG(ERROR) << "Error stat-ing " << fd;
    return rc;
  }
  if (S_ISREG(stbuf.st_mode))
    return stbuf.st_size;
  if (S_ISBLK(stbuf.st_mode))
    return BlockDevSize(fd);
  LOG(ERROR) << "Couldn't determine the type of " << fd;
  return -1;
}

off_t FileSize(const string& path) {
  int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    PLOG(ERROR) << "Error opening " << path;
    return fd;
  }
  off_t size = FileSize(fd);
  if (size == -1)
    PLOG(ERROR) << "Error getting file size of " << path;
  close(fd);
  return size;
}

void HexDumpArray(const uint8_t* const arr, const size_t length) {
  LOG(INFO) << "Logging array of length: " << length;
  const unsigned int bytes_per_line = 16;
  for (uint32_t i = 0; i < length; i += bytes_per_line) {
    const unsigned int bytes_remaining = length - i;
    const unsigned int bytes_per_this_line = min(bytes_per_line,
                                                 bytes_remaining);
    char header[100];
    int r = snprintf(header, sizeof(header), "0x%08x : ", i);
    TEST_AND_RETURN(r == 13);
    string line = header;
    for (unsigned int j = 0; j < bytes_per_this_line; j++) {
      char buf[20];
      uint8_t c = arr[i + j];
      r = snprintf(buf, sizeof(buf), "%02x ", static_cast<unsigned int>(c));
      TEST_AND_RETURN(r == 3);
      line += buf;
    }
    LOG(INFO) << line;
  }
}

string GetDiskName(const string& partition_name) {
  string disk_name;
  return SplitPartitionName(partition_name, &disk_name, nullptr) ?
      disk_name : string();
}

int GetPartitionNumber(const string& partition_name) {
  int partition_num = 0;
  return SplitPartitionName(partition_name, nullptr, &partition_num) ?
      partition_num : 0;
}

bool SplitPartitionName(const string& partition_name,
                        string* out_disk_name,
                        int* out_partition_num) {
  if (!base::StartsWithASCII(partition_name, "/dev/", true)) {
    LOG(ERROR) << "Invalid partition device name: " << partition_name;
    return false;
  }

  size_t last_nondigit_pos = partition_name.find_last_not_of("0123456789");
  if (last_nondigit_pos == string::npos ||
      (last_nondigit_pos + 1) == partition_name.size()) {
    LOG(ERROR) << "Unable to parse partition device name: " << partition_name;
    return false;
  }

  size_t partition_name_len = string::npos;
  if (partition_name[last_nondigit_pos] == '_') {
    // NAND block devices have weird naming which could be something
    // like "/dev/ubiblock2_0". We discard "_0" in such a case.
    size_t prev_nondigit_pos =
        partition_name.find_last_not_of("0123456789", last_nondigit_pos - 1);
    if (prev_nondigit_pos == string::npos ||
        (prev_nondigit_pos + 1) == last_nondigit_pos) {
      LOG(ERROR) << "Unable to parse partition device name: " << partition_name;
      return false;
    }

    partition_name_len = last_nondigit_pos - prev_nondigit_pos;
    last_nondigit_pos = prev_nondigit_pos;
  }

  if (out_disk_name) {
    // Special case for MMC devices which have the following naming scheme:
    // mmcblk0p2
    size_t disk_name_len = last_nondigit_pos;
    if (partition_name[last_nondigit_pos] != 'p' ||
        last_nondigit_pos == 0 ||
        !isdigit(partition_name[last_nondigit_pos - 1])) {
      disk_name_len++;
    }
    *out_disk_name = partition_name.substr(0, disk_name_len);
  }

  if (out_partition_num) {
    string partition_str = partition_name.substr(last_nondigit_pos + 1,
                                                 partition_name_len);
    *out_partition_num = atoi(partition_str.c_str());
  }
  return true;
}

string MakePartitionName(const string& disk_name, int partition_num) {
  if (partition_num < 1) {
    LOG(ERROR) << "Invalid partition number: " << partition_num;
    return string();
  }

  if (!base::StartsWithASCII(disk_name, "/dev/", true)) {
    LOG(ERROR) << "Invalid disk name: " << disk_name;
    return string();
  }

  if (IsMtdDeviceName(disk_name)) {
    // Special case for UBI block devices.
    //   1. ubiblock is not writable, we need to use plain "ubi".
    //   2. There is a "_0" suffix.
    return MakeNandPartitionName(partition_num);
  }

  string partition_name = disk_name;
  if (isdigit(partition_name.back())) {
    // Special case for devices with names ending with a digit.
    // Add "p" to separate the disk name from partition number,
    // e.g. "/dev/loop0p2"
    partition_name += 'p';
  }

  partition_name += std::to_string(partition_num);

  return partition_name;
}

string MakePartitionNameForMount(const string& part_name) {
  if (IsMtdDeviceName(part_name)) {
    int partition_num;
    if (!SplitPartitionName(part_name, nullptr, &partition_num)) {
      return "";
    }
    return MakeNandPartitionNameForMount(partition_num);
  }
  return part_name;
}

string SysfsBlockDevice(const string& device) {
  base::FilePath device_path(device);
  if (device_path.DirName().value() != "/dev") {
    return "";
  }
  return base::FilePath("/sys/block").Append(device_path.BaseName()).value();
}

bool IsRemovableDevice(const string& device) {
  string sysfs_block = SysfsBlockDevice(device);
  string removable;
  if (sysfs_block.empty() ||
      !base::ReadFileToString(base::FilePath(sysfs_block).Append("removable"),
                              &removable)) {
    return false;
  }
  base::TrimWhitespaceASCII(removable, base::TRIM_ALL, &removable);
  return removable == "1";
}

string ErrnoNumberAsString(int err) {
  char buf[100];
  buf[0] = '\0';
  return strerror_r(err, buf, sizeof(buf));
}

bool FileExists(const char* path) {
  struct stat stbuf;
  return 0 == lstat(path, &stbuf);
}

bool IsSymlink(const char* path) {
  struct stat stbuf;
  return lstat(path, &stbuf) == 0 && S_ISLNK(stbuf.st_mode) != 0;
}

bool TryAttachingUbiVolume(int volume_num, int timeout) {
  const string volume_path = base::StringPrintf("/dev/ubi%d_0", volume_num);
  if (FileExists(volume_path.c_str())) {
    return true;
  }

  int exit_code;
  vector<string> cmd = {
      "ubiattach",
      "-m",
      base::StringPrintf("%d", volume_num),
      "-d",
      base::StringPrintf("%d", volume_num)
  };
  TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &exit_code, nullptr));
  TEST_AND_RETURN_FALSE(exit_code == 0);

  cmd = {
      "ubiblock",
      "--create",
      volume_path
  };
  TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &exit_code, nullptr));
  TEST_AND_RETURN_FALSE(exit_code == 0);

  while (timeout > 0 && !FileExists(volume_path.c_str())) {
    sleep(1);
    timeout--;
  }

  return FileExists(volume_path.c_str());
}

// If |path| is absolute, or explicit relative to the current working directory,
// leaves it as is. Otherwise, if TMPDIR is defined in the environment and is
// non-empty, prepends it to |path|. Otherwise, prepends /tmp.  Returns the
// resulting path.
static const string PrependTmpdir(const string& path) {
  if (path[0] == '/' || base::StartsWithASCII(path, "./", true) ||
      base::StartsWithASCII(path, "../", true))
    return path;

  const char *tmpdir = getenv("TMPDIR");
  const string prefix = (tmpdir && *tmpdir ? tmpdir : "/tmp");
  return prefix + "/" + path;
}

bool MakeTempFile(const string& base_filename_template,
                  string* filename,
                  int* fd) {
  const string filename_template = PrependTmpdir(base_filename_template);
  DCHECK(filename || fd);
  vector<char> buf(filename_template.size() + 1);
  memcpy(buf.data(), filename_template.data(), filename_template.size());
  buf[filename_template.size()] = '\0';

  int mkstemp_fd = mkstemp(buf.data());
  TEST_AND_RETURN_FALSE_ERRNO(mkstemp_fd >= 0);
  if (filename) {
    *filename = buf.data();
  }
  if (fd) {
    *fd = mkstemp_fd;
  } else {
    close(mkstemp_fd);
  }
  return true;
}

bool MakeTempDirectory(const string& base_dirname_template,
                       string* dirname) {
  const string dirname_template = PrependTmpdir(base_dirname_template);
  DCHECK(dirname);
  vector<char> buf(dirname_template.size() + 1);
  memcpy(buf.data(), dirname_template.data(), dirname_template.size());
  buf[dirname_template.size()] = '\0';

  char* return_code = mkdtemp(buf.data());
  TEST_AND_RETURN_FALSE_ERRNO(return_code != nullptr);
  *dirname = buf.data();
  return true;
}

bool MountFilesystem(const string& device,
                     const string& mountpoint,
                     unsigned long mountflags) {  // NOLINT(runtime/int)
  // TODO(sosa): Remove "ext3" once crbug.com/208022 is resolved.
  const vector<const char*> fstypes{"ext2", "ext3", "squashfs"};
  for (const char* fstype : fstypes) {
    int rc = mount(device.c_str(), mountpoint.c_str(), fstype, mountflags,
                   nullptr);
    if (rc == 0)
      return true;

    PLOG(WARNING) << "Unable to mount destination device " << device
                  << " on " << mountpoint << " as " << fstype;
  }
  LOG(ERROR) << "Unable to mount " << device << " with any supported type";
  return false;
}

bool UnmountFilesystem(const string& mountpoint) {
  for (int num_retries = 0; ; ++num_retries) {
    if (umount(mountpoint.c_str()) == 0)
      break;

    TEST_AND_RETURN_FALSE_ERRNO(errno == EBUSY &&
                                num_retries < kUnmountMaxNumOfRetries);
    usleep(kUnmountRetryIntervalInMicroseconds);
  }
  return true;
}

bool GetFilesystemSize(const string& device,
                       int* out_block_count,
                       int* out_block_size) {
  int fd = HANDLE_EINTR(open(device.c_str(), O_RDONLY));
  TEST_AND_RETURN_FALSE_ERRNO(fd >= 0);
  ScopedFdCloser fd_closer(&fd);
  return GetFilesystemSizeFromFD(fd, out_block_count, out_block_size);
}

bool GetFilesystemSizeFromFD(int fd,
                             int* out_block_count,
                             int* out_block_size) {
  TEST_AND_RETURN_FALSE(fd >= 0);

  // Determine the filesystem size by directly reading the block count and
  // block size information from the superblock. Supported FS are ext3 and
  // squashfs.

  // Read from the fd only once and detect in memory. The first 2 KiB is enough
  // to read the ext2 superblock (located at offset 1024) and the squashfs
  // superblock (located at offset 0).
  const ssize_t kBufferSize = 2048;

  uint8_t buffer[kBufferSize];
  if (HANDLE_EINTR(pread(fd, buffer, kBufferSize, 0)) != kBufferSize) {
    PLOG(ERROR) << "Unable to read the file system header:";
    return false;
  }

  if (GetSquashfs4Size(buffer, kBufferSize, out_block_count, out_block_size))
    return true;
  if (GetExt3Size(buffer, kBufferSize, out_block_count, out_block_size))
    return true;

  LOG(ERROR) << "Unable to determine file system type.";
  return false;
}

bool GetExt3Size(const uint8_t* buffer, size_t buffer_size,
                 int* out_block_count,
                 int* out_block_size) {
  // See include/linux/ext2_fs.h for more details on the structure. We obtain
  // ext2 constants from ext2fs/ext2fs.h header but we don't link with the
  // library.
  if (buffer_size < SUPERBLOCK_OFFSET + SUPERBLOCK_SIZE)
    return false;

  const uint8_t* superblock = buffer + SUPERBLOCK_OFFSET;

  // ext3_fs.h: ext3_super_block.s_blocks_count
  uint32_t block_count =
      *reinterpret_cast<const uint32_t*>(superblock + 1 * sizeof(int32_t));

  // ext3_fs.h: ext3_super_block.s_log_block_size
  uint32_t log_block_size =
      *reinterpret_cast<const uint32_t*>(superblock + 6 * sizeof(int32_t));

  // ext3_fs.h: ext3_super_block.s_magic
  uint16_t magic =
      *reinterpret_cast<const uint16_t*>(superblock + 14 * sizeof(int32_t));

  block_count = le32toh(block_count);
  log_block_size = le32toh(log_block_size) + EXT2_MIN_BLOCK_LOG_SIZE;
  magic = le16toh(magic);

  // Sanity check the parameters.
  TEST_AND_RETURN_FALSE(magic == EXT2_SUPER_MAGIC);
  TEST_AND_RETURN_FALSE(log_block_size >= EXT2_MIN_BLOCK_LOG_SIZE &&
                        log_block_size <= EXT2_MAX_BLOCK_LOG_SIZE);
  TEST_AND_RETURN_FALSE(block_count > 0);

  if (out_block_count)
    *out_block_count = block_count;
  if (out_block_size)
    *out_block_size = 1 << log_block_size;
  return true;
}

bool GetSquashfs4Size(const uint8_t* buffer, size_t buffer_size,
                      int* out_block_count,
                      int* out_block_size) {
  // See fs/squashfs/squashfs_fs.h for format details. We only support
  // Squashfs 4.x little endian.

  // sizeof(struct squashfs_super_block)
  const size_t kSquashfsSuperBlockSize = 96;
  if (buffer_size < kSquashfsSuperBlockSize)
    return false;

  // Check magic, squashfs_fs.h: SQUASHFS_MAGIC
  if (memcmp(buffer, "hsqs", 4) != 0)
    return false;  // Only little endian is supported.

  // squashfs_fs.h: struct squashfs_super_block.s_major
  uint16_t s_major = *reinterpret_cast<const uint16_t*>(
      buffer + 5 * sizeof(uint32_t) + 4 * sizeof(uint16_t));

  if (s_major != 4) {
    LOG(ERROR) << "Found unsupported squashfs major version " << s_major;
    return false;
  }

  // squashfs_fs.h: struct squashfs_super_block.bytes_used
  uint64_t bytes_used = *reinterpret_cast<const int64_t*>(
      buffer + 5 * sizeof(uint32_t) + 6 * sizeof(uint16_t) + sizeof(uint64_t));

  const int block_size = 4096;

  // The squashfs' bytes_used doesn't need to be aligned with the block boundary
  // so we round up to the nearest blocksize.
  if (out_block_count)
    *out_block_count = (bytes_used + block_size - 1) / block_size;
  if (out_block_size)
    *out_block_size = block_size;
  return true;
}

bool IsExtFilesystem(const string& device) {
  chromeos::Blob header;
  // The first 2 KiB is enough to read the ext2 superblock (located at offset
  // 1024).
  if (!ReadFileChunk(device, 0, 2048, &header))
    return false;
  return GetExt3Size(header.data(), header.size(), nullptr, nullptr);
}

bool IsSquashfsFilesystem(const string& device) {
  chromeos::Blob header;
  // The first 96 is enough to read the squashfs superblock.
  const ssize_t kSquashfsSuperBlockSize = 96;
  if (!ReadFileChunk(device, 0, kSquashfsSuperBlockSize, &header))
    return false;
  return GetSquashfs4Size(header.data(), header.size(), nullptr, nullptr);
}

// Tries to parse the header of an ELF file to obtain a human-readable
// description of it on the |output| string.
static bool GetFileFormatELF(const uint8_t* buffer, size_t size,
                             string* output) {
  // 0x00: EI_MAG - ELF magic header, 4 bytes.
  if (size < SELFMAG || memcmp(buffer, ELFMAG, SELFMAG) != 0)
    return false;
  *output = "ELF";

  // 0x04: EI_CLASS, 1 byte.
  if (size < EI_CLASS + 1)
    return true;
  switch (buffer[EI_CLASS]) {
    case ELFCLASS32:
      *output += " 32-bit";
      break;
    case ELFCLASS64:
      *output += " 64-bit";
      break;
    default:
      *output += " ?-bit";
  }

  // 0x05: EI_DATA, endianness, 1 byte.
  if (size < EI_DATA + 1)
    return true;
  uint8_t ei_data = buffer[EI_DATA];
  switch (ei_data) {
    case ELFDATA2LSB:
      *output += " little-endian";
      break;
    case ELFDATA2MSB:
      *output += " big-endian";
      break;
    default:
      *output += " ?-endian";
      // Don't parse anything after the 0x10 offset if endianness is unknown.
      return true;
  }

  const Elf32_Ehdr* hdr = reinterpret_cast<const Elf32_Ehdr*>(buffer);
  // 0x12: e_machine, 2 byte endianness based on ei_data. The position (0x12)
  // and size is the same for both 32 and 64 bits.
  if (size < offsetof(Elf32_Ehdr, e_machine) + sizeof(hdr->e_machine))
    return true;
  uint16_t e_machine;
  // Fix endianess regardless of the host endianess.
  if (ei_data == ELFDATA2LSB)
    e_machine = le16toh(hdr->e_machine);
  else
    e_machine = be16toh(hdr->e_machine);

  switch (e_machine) {
    case EM_386:
      *output += " x86";
      break;
    case EM_MIPS:
      *output += " mips";
      break;
    case EM_ARM:
      *output += " arm";
      break;
    case EM_X86_64:
      *output += " x86-64";
      break;
    default:
      *output += " unknown-arch";
  }
  return true;
}

string GetFileFormat(const string& path) {
  chromeos::Blob buffer;
  if (!ReadFileChunkAndAppend(path, 0, kGetFileFormatMaxHeaderSize, &buffer))
    return "File not found.";

  string result;
  if (GetFileFormatELF(buffer.data(), buffer.size(), &result))
    return result;

  return "data";
}

namespace {
// Do the actual trigger. We do it as a main-loop callback to (try to) get a
// consistent stack trace.
void TriggerCrashReporterUpload() {
  pid_t pid = fork();
  CHECK_GE(pid, 0) << "fork failed";  // fork() failed. Something is very wrong.
  if (pid == 0) {
    // We are the child. Crash.
    abort();  // never returns
  }
  // We are the parent. Wait for child to terminate.
  pid_t result = waitpid(pid, nullptr, 0);
  LOG_IF(ERROR, result < 0) << "waitpid() failed";
}
}  // namespace

void ScheduleCrashReporterUpload() {
  chromeos::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TriggerCrashReporterUpload));
}

bool SetCpuShares(CpuShares shares) {
  string string_shares = base::IntToString(static_cast<int>(shares));
  string cpu_shares_file = string(utils::kCGroupDir) + "/cpu.shares";
  LOG(INFO) << "Setting cgroup cpu shares to  " << string_shares;
  if (utils::WriteFile(cpu_shares_file.c_str(), string_shares.c_str(),
                       string_shares.size())) {
    return true;
  } else {
    LOG(ERROR) << "Failed to change cgroup cpu shares to "<< string_shares
               << " using " << cpu_shares_file;
    return false;
  }
}

int FuzzInt(int value, unsigned int range) {
  int min = value - range / 2;
  int max = value + range - range / 2;
  return base::RandInt(min, max);
}

string FormatSecs(unsigned secs) {
  return FormatTimeDelta(TimeDelta::FromSeconds(secs));
}

string FormatTimeDelta(TimeDelta delta) {
  string str;

  // Handle negative durations by prefixing with a minus.
  if (delta.ToInternalValue() < 0) {
    delta *= -1;
    str = "-";
  }

  // Canonicalize into days, hours, minutes, seconds and microseconds.
  unsigned days = delta.InDays();
  delta -= TimeDelta::FromDays(days);
  unsigned hours = delta.InHours();
  delta -= TimeDelta::FromHours(hours);
  unsigned mins = delta.InMinutes();
  delta -= TimeDelta::FromMinutes(mins);
  unsigned secs = delta.InSeconds();
  delta -= TimeDelta::FromSeconds(secs);
  unsigned usecs = delta.InMicroseconds();

  if (days)
    base::StringAppendF(&str, "%ud", days);
  if (days || hours)
    base::StringAppendF(&str, "%uh", hours);
  if (days || hours || mins)
    base::StringAppendF(&str, "%um", mins);
  base::StringAppendF(&str, "%u", secs);
  if (usecs) {
    int width = 6;
    while ((usecs / 10) * 10 == usecs) {
      usecs /= 10;
      width--;
    }
    base::StringAppendF(&str, ".%0*u", width, usecs);
  }
  base::StringAppendF(&str, "s");
  return str;
}

string ToString(const Time utc_time) {
  Time::Exploded exp_time;
  utc_time.UTCExplode(&exp_time);
  return base::StringPrintf("%d/%d/%d %d:%02d:%02d GMT",
                      exp_time.month,
                      exp_time.day_of_month,
                      exp_time.year,
                      exp_time.hour,
                      exp_time.minute,
                      exp_time.second);
}

string ToString(bool b) {
  return (b ? "true" : "false");
}

string ToString(DownloadSource source) {
  switch (source) {
    case kDownloadSourceHttpsServer: return "HttpsServer";
    case kDownloadSourceHttpServer:  return "HttpServer";
    case kDownloadSourceHttpPeer:    return "HttpPeer";
    case kNumDownloadSources:        return "Unknown";
    // Don't add a default case to let the compiler warn about newly added
    // download sources which should be added here.
  }

  return "Unknown";
}

string ToString(PayloadType payload_type) {
  switch (payload_type) {
    case kPayloadTypeDelta:      return "Delta";
    case kPayloadTypeFull:       return "Full";
    case kPayloadTypeForcedFull: return "ForcedFull";
    case kNumPayloadTypes:       return "Unknown";
    // Don't add a default case to let the compiler warn about newly added
    // payload types which should be added here.
  }

  return "Unknown";
}

ErrorCode GetBaseErrorCode(ErrorCode code) {
  // Ignore the higher order bits in the code by applying the mask as
  // we want the enumerations to be in the small contiguous range
  // with values less than ErrorCode::kUmaReportedMax.
  ErrorCode base_code = static_cast<ErrorCode>(
      static_cast<int>(code) & ~static_cast<int>(ErrorCode::kSpecialFlags));

  // Make additional adjustments required for UMA and error classification.
  // TODO(jaysri): Move this logic to UeErrorCode.cc when we fix
  // chromium-os:34369.
  if (base_code >= ErrorCode::kOmahaRequestHTTPResponseBase) {
    // Since we want to keep the enums to a small value, aggregate all HTTP
    // errors into this one bucket for UMA and error classification purposes.
    LOG(INFO) << "Converting error code " << base_code
              << " to ErrorCode::kOmahaErrorInHTTPResponse";
    base_code = ErrorCode::kOmahaErrorInHTTPResponse;
  }

  return base_code;
}

metrics::AttemptResult GetAttemptResult(ErrorCode code) {
  ErrorCode base_code = static_cast<ErrorCode>(
      static_cast<int>(code) & ~static_cast<int>(ErrorCode::kSpecialFlags));

  switch (base_code) {
    case ErrorCode::kSuccess:
      return metrics::AttemptResult::kUpdateSucceeded;

    case ErrorCode::kDownloadTransferError:
      return metrics::AttemptResult::kPayloadDownloadError;

    case ErrorCode::kDownloadInvalidMetadataSize:
    case ErrorCode::kDownloadInvalidMetadataMagicString:
    case ErrorCode::kDownloadMetadataSignatureError:
    case ErrorCode::kDownloadMetadataSignatureVerificationError:
    case ErrorCode::kPayloadMismatchedType:
    case ErrorCode::kUnsupportedMajorPayloadVersion:
    case ErrorCode::kUnsupportedMinorPayloadVersion:
    case ErrorCode::kDownloadNewPartitionInfoError:
    case ErrorCode::kDownloadSignatureMissingInManifest:
    case ErrorCode::kDownloadManifestParseError:
    case ErrorCode::kDownloadOperationHashMissingError:
      return metrics::AttemptResult::kMetadataMalformed;

    case ErrorCode::kDownloadOperationHashMismatch:
    case ErrorCode::kDownloadOperationHashVerificationError:
      return metrics::AttemptResult::kOperationMalformed;

    case ErrorCode::kDownloadOperationExecutionError:
    case ErrorCode::kInstallDeviceOpenError:
    case ErrorCode::kKernelDeviceOpenError:
    case ErrorCode::kDownloadWriteError:
    case ErrorCode::kFilesystemCopierError:
    case ErrorCode::kFilesystemVerifierError:
      return metrics::AttemptResult::kOperationExecutionError;

    case ErrorCode::kDownloadMetadataSignatureMismatch:
      return metrics::AttemptResult::kMetadataVerificationFailed;

    case ErrorCode::kPayloadSizeMismatchError:
    case ErrorCode::kPayloadHashMismatchError:
    case ErrorCode::kDownloadPayloadVerificationError:
    case ErrorCode::kSignedDeltaPayloadExpectedError:
    case ErrorCode::kDownloadPayloadPubKeyVerificationError:
      return metrics::AttemptResult::kPayloadVerificationFailed;

    case ErrorCode::kNewRootfsVerificationError:
    case ErrorCode::kNewKernelVerificationError:
      return metrics::AttemptResult::kVerificationFailed;

    case ErrorCode::kPostinstallRunnerError:
    case ErrorCode::kPostinstallBootedFromFirmwareB:
    case ErrorCode::kPostinstallFirmwareRONotUpdatable:
      return metrics::AttemptResult::kPostInstallFailed;

    // We should never get these errors in the update-attempt stage so
    // return internal error if this happens.
    case ErrorCode::kError:
    case ErrorCode::kOmahaRequestXMLParseError:
    case ErrorCode::kOmahaRequestError:
    case ErrorCode::kOmahaResponseHandlerError:
    case ErrorCode::kDownloadStateInitializationError:
    case ErrorCode::kOmahaRequestEmptyResponseError:
    case ErrorCode::kDownloadInvalidMetadataSignature:
    case ErrorCode::kOmahaResponseInvalid:
    case ErrorCode::kOmahaUpdateIgnoredPerPolicy:
    case ErrorCode::kOmahaUpdateDeferredPerPolicy:
    case ErrorCode::kOmahaErrorInHTTPResponse:
    case ErrorCode::kDownloadMetadataSignatureMissingError:
    case ErrorCode::kOmahaUpdateDeferredForBackoff:
    case ErrorCode::kPostinstallPowerwashError:
    case ErrorCode::kUpdateCanceledByChannelChange:
    case ErrorCode::kOmahaRequestXMLHasEntityDecl:
      return metrics::AttemptResult::kInternalError;

    // Special flags. These can't happen (we mask them out above) but
    // the compiler doesn't know that. Just break out so we can warn and
    // return |kInternalError|.
    case ErrorCode::kUmaReportedMax:
    case ErrorCode::kOmahaRequestHTTPResponseBase:
    case ErrorCode::kDevModeFlag:
    case ErrorCode::kResumedFlag:
    case ErrorCode::kTestImageFlag:
    case ErrorCode::kTestOmahaUrlFlag:
    case ErrorCode::kSpecialFlags:
      break;
  }

  LOG(ERROR) << "Unexpected error code " << base_code;
  return metrics::AttemptResult::kInternalError;
}


metrics::DownloadErrorCode GetDownloadErrorCode(ErrorCode code) {
  ErrorCode base_code = static_cast<ErrorCode>(
      static_cast<int>(code) & ~static_cast<int>(ErrorCode::kSpecialFlags));

  if (base_code >= ErrorCode::kOmahaRequestHTTPResponseBase) {
    int http_status =
        static_cast<int>(base_code) -
        static_cast<int>(ErrorCode::kOmahaRequestHTTPResponseBase);
    if (http_status >= 200 && http_status <= 599) {
      return static_cast<metrics::DownloadErrorCode>(
          static_cast<int>(metrics::DownloadErrorCode::kHttpStatus200) +
          http_status - 200);
    } else if (http_status == 0) {
      // The code is using HTTP Status 0 for "Unable to get http
      // response code."
      return metrics::DownloadErrorCode::kDownloadError;
    }
    LOG(WARNING) << "Unexpected HTTP status code " << http_status;
    return metrics::DownloadErrorCode::kHttpStatusOther;
  }

  switch (base_code) {
    // Unfortunately, ErrorCode::kDownloadTransferError is returned for a wide
    // variety of errors (proxy errors, host not reachable, timeouts etc.).
    //
    // For now just map that to kDownloading. See http://crbug.com/355745
    // for how we plan to add more detail in the future.
    case ErrorCode::kDownloadTransferError:
      return metrics::DownloadErrorCode::kDownloadError;

    // All of these error codes are not related to downloading so break
    // out so we can warn and return InputMalformed.
    case ErrorCode::kSuccess:
    case ErrorCode::kError:
    case ErrorCode::kOmahaRequestError:
    case ErrorCode::kOmahaResponseHandlerError:
    case ErrorCode::kFilesystemCopierError:
    case ErrorCode::kPostinstallRunnerError:
    case ErrorCode::kPayloadMismatchedType:
    case ErrorCode::kInstallDeviceOpenError:
    case ErrorCode::kKernelDeviceOpenError:
    case ErrorCode::kPayloadHashMismatchError:
    case ErrorCode::kPayloadSizeMismatchError:
    case ErrorCode::kDownloadPayloadVerificationError:
    case ErrorCode::kDownloadNewPartitionInfoError:
    case ErrorCode::kDownloadWriteError:
    case ErrorCode::kNewRootfsVerificationError:
    case ErrorCode::kNewKernelVerificationError:
    case ErrorCode::kSignedDeltaPayloadExpectedError:
    case ErrorCode::kDownloadPayloadPubKeyVerificationError:
    case ErrorCode::kPostinstallBootedFromFirmwareB:
    case ErrorCode::kDownloadStateInitializationError:
    case ErrorCode::kDownloadInvalidMetadataMagicString:
    case ErrorCode::kDownloadSignatureMissingInManifest:
    case ErrorCode::kDownloadManifestParseError:
    case ErrorCode::kDownloadMetadataSignatureError:
    case ErrorCode::kDownloadMetadataSignatureVerificationError:
    case ErrorCode::kDownloadMetadataSignatureMismatch:
    case ErrorCode::kDownloadOperationHashVerificationError:
    case ErrorCode::kDownloadOperationExecutionError:
    case ErrorCode::kDownloadOperationHashMismatch:
    case ErrorCode::kOmahaRequestEmptyResponseError:
    case ErrorCode::kOmahaRequestXMLParseError:
    case ErrorCode::kDownloadInvalidMetadataSize:
    case ErrorCode::kDownloadInvalidMetadataSignature:
    case ErrorCode::kOmahaResponseInvalid:
    case ErrorCode::kOmahaUpdateIgnoredPerPolicy:
    case ErrorCode::kOmahaUpdateDeferredPerPolicy:
    case ErrorCode::kOmahaErrorInHTTPResponse:
    case ErrorCode::kDownloadOperationHashMissingError:
    case ErrorCode::kDownloadMetadataSignatureMissingError:
    case ErrorCode::kOmahaUpdateDeferredForBackoff:
    case ErrorCode::kPostinstallPowerwashError:
    case ErrorCode::kUpdateCanceledByChannelChange:
    case ErrorCode::kPostinstallFirmwareRONotUpdatable:
    case ErrorCode::kUnsupportedMajorPayloadVersion:
    case ErrorCode::kUnsupportedMinorPayloadVersion:
    case ErrorCode::kOmahaRequestXMLHasEntityDecl:
    case ErrorCode::kFilesystemVerifierError:
      break;

    // Special flags. These can't happen (we mask them out above) but
    // the compiler doesn't know that. Just break out so we can warn and
    // return |kInputMalformed|.
    case ErrorCode::kUmaReportedMax:
    case ErrorCode::kOmahaRequestHTTPResponseBase:
    case ErrorCode::kDevModeFlag:
    case ErrorCode::kResumedFlag:
    case ErrorCode::kTestImageFlag:
    case ErrorCode::kTestOmahaUrlFlag:
    case ErrorCode::kSpecialFlags:
      LOG(ERROR) << "Unexpected error code " << base_code;
      break;
  }

  return metrics::DownloadErrorCode::kInputMalformed;
}

metrics::ConnectionType GetConnectionType(
    NetworkConnectionType type,
    NetworkTethering tethering) {
  switch (type) {
    case NetworkConnectionType::kUnknown:
      return metrics::ConnectionType::kUnknown;

    case NetworkConnectionType::kEthernet:
      if (tethering == NetworkTethering::kConfirmed)
        return metrics::ConnectionType::kTetheredEthernet;
      else
        return metrics::ConnectionType::kEthernet;

    case NetworkConnectionType::kWifi:
      if (tethering == NetworkTethering::kConfirmed)
        return metrics::ConnectionType::kTetheredWifi;
      else
        return metrics::ConnectionType::kWifi;

    case NetworkConnectionType::kWimax:
      return metrics::ConnectionType::kWimax;

    case NetworkConnectionType::kBluetooth:
      return metrics::ConnectionType::kBluetooth;

    case NetworkConnectionType::kCellular:
      return metrics::ConnectionType::kCellular;
  }

  LOG(ERROR) << "Unexpected network connection type: type="
             << static_cast<int>(type)
             << ", tethering=" << static_cast<int>(tethering);

  return metrics::ConnectionType::kUnknown;
}

// Returns a printable version of the various flags denoted in the higher order
// bits of the given code. Returns an empty string if none of those bits are
// set.
string GetFlagNames(uint32_t code) {
  uint32_t flags = (static_cast<uint32_t>(code) &
                    static_cast<uint32_t>(ErrorCode::kSpecialFlags));
  string flag_names;
  string separator = "";
  for (size_t i = 0; i < sizeof(flags) * 8; i++) {
    uint32_t flag = flags & (1 << i);
    if (flag) {
      flag_names += separator + CodeToString(static_cast<ErrorCode>(flag));
      separator = ", ";
    }
  }

  return flag_names;
}

void SendErrorCodeToUma(SystemState* system_state, ErrorCode code) {
  if (!system_state)
    return;

  ErrorCode uma_error_code = GetBaseErrorCode(code);

  // If the code doesn't have flags computed already, compute them now based on
  // the state of the current update attempt.
  uint32_t flags =
      static_cast<int>(code) & static_cast<int>(ErrorCode::kSpecialFlags);
  if (!flags)
    flags = system_state->update_attempter()->GetErrorCodeFlags();

  // Determine the UMA bucket depending on the flags. But, ignore the resumed
  // flag, as it's perfectly normal for production devices to resume their
  // downloads and so we want to record those cases also in NormalErrorCodes
  // bucket.
  string metric =
      flags & ~static_cast<uint32_t>(ErrorCode::kResumedFlag) ?
      "Installer.DevModeErrorCodes" : "Installer.NormalErrorCodes";

  LOG(INFO) << "Sending error code " << uma_error_code
            << " (" << CodeToString(uma_error_code) << ")"
            << " to UMA metric: " << metric
            << ". Flags = " << (flags ? GetFlagNames(flags) : "None");

  system_state->metrics_lib()->SendEnumToUMA(
      metric, static_cast<int>(uma_error_code),
      static_cast<int>(ErrorCode::kUmaReportedMax));
}

string CodeToString(ErrorCode code) {
  // If the given code has both parts (i.e. the error code part and the flags
  // part) then strip off the flags part since the switch statement below
  // has case statements only for the base error code or a single flag but
  // doesn't support any combinations of those.
  if ((static_cast<int>(code) & static_cast<int>(ErrorCode::kSpecialFlags)) &&
      (static_cast<int>(code) & ~static_cast<int>(ErrorCode::kSpecialFlags)))
    code = static_cast<ErrorCode>(
        static_cast<int>(code) & ~static_cast<int>(ErrorCode::kSpecialFlags));
  switch (code) {
    case ErrorCode::kSuccess: return "ErrorCode::kSuccess";
    case ErrorCode::kError: return "ErrorCode::kError";
    case ErrorCode::kOmahaRequestError: return "ErrorCode::kOmahaRequestError";
    case ErrorCode::kOmahaResponseHandlerError:
      return "ErrorCode::kOmahaResponseHandlerError";
    case ErrorCode::kFilesystemCopierError:
      return "ErrorCode::kFilesystemCopierError";
    case ErrorCode::kPostinstallRunnerError:
      return "ErrorCode::kPostinstallRunnerError";
    case ErrorCode::kPayloadMismatchedType:
      return "ErrorCode::kPayloadMismatchedType";
    case ErrorCode::kInstallDeviceOpenError:
      return "ErrorCode::kInstallDeviceOpenError";
    case ErrorCode::kKernelDeviceOpenError:
      return "ErrorCode::kKernelDeviceOpenError";
    case ErrorCode::kDownloadTransferError:
      return "ErrorCode::kDownloadTransferError";
    case ErrorCode::kPayloadHashMismatchError:
      return "ErrorCode::kPayloadHashMismatchError";
    case ErrorCode::kPayloadSizeMismatchError:
      return "ErrorCode::kPayloadSizeMismatchError";
    case ErrorCode::kDownloadPayloadVerificationError:
      return "ErrorCode::kDownloadPayloadVerificationError";
    case ErrorCode::kDownloadNewPartitionInfoError:
      return "ErrorCode::kDownloadNewPartitionInfoError";
    case ErrorCode::kDownloadWriteError:
      return "ErrorCode::kDownloadWriteError";
    case ErrorCode::kNewRootfsVerificationError:
      return "ErrorCode::kNewRootfsVerificationError";
    case ErrorCode::kNewKernelVerificationError:
      return "ErrorCode::kNewKernelVerificationError";
    case ErrorCode::kSignedDeltaPayloadExpectedError:
      return "ErrorCode::kSignedDeltaPayloadExpectedError";
    case ErrorCode::kDownloadPayloadPubKeyVerificationError:
      return "ErrorCode::kDownloadPayloadPubKeyVerificationError";
    case ErrorCode::kPostinstallBootedFromFirmwareB:
      return "ErrorCode::kPostinstallBootedFromFirmwareB";
    case ErrorCode::kDownloadStateInitializationError:
      return "ErrorCode::kDownloadStateInitializationError";
    case ErrorCode::kDownloadInvalidMetadataMagicString:
      return "ErrorCode::kDownloadInvalidMetadataMagicString";
    case ErrorCode::kDownloadSignatureMissingInManifest:
      return "ErrorCode::kDownloadSignatureMissingInManifest";
    case ErrorCode::kDownloadManifestParseError:
      return "ErrorCode::kDownloadManifestParseError";
    case ErrorCode::kDownloadMetadataSignatureError:
      return "ErrorCode::kDownloadMetadataSignatureError";
    case ErrorCode::kDownloadMetadataSignatureVerificationError:
      return "ErrorCode::kDownloadMetadataSignatureVerificationError";
    case ErrorCode::kDownloadMetadataSignatureMismatch:
      return "ErrorCode::kDownloadMetadataSignatureMismatch";
    case ErrorCode::kDownloadOperationHashVerificationError:
      return "ErrorCode::kDownloadOperationHashVerificationError";
    case ErrorCode::kDownloadOperationExecutionError:
      return "ErrorCode::kDownloadOperationExecutionError";
    case ErrorCode::kDownloadOperationHashMismatch:
      return "ErrorCode::kDownloadOperationHashMismatch";
    case ErrorCode::kOmahaRequestEmptyResponseError:
      return "ErrorCode::kOmahaRequestEmptyResponseError";
    case ErrorCode::kOmahaRequestXMLParseError:
      return "ErrorCode::kOmahaRequestXMLParseError";
    case ErrorCode::kDownloadInvalidMetadataSize:
      return "ErrorCode::kDownloadInvalidMetadataSize";
    case ErrorCode::kDownloadInvalidMetadataSignature:
      return "ErrorCode::kDownloadInvalidMetadataSignature";
    case ErrorCode::kOmahaResponseInvalid:
      return "ErrorCode::kOmahaResponseInvalid";
    case ErrorCode::kOmahaUpdateIgnoredPerPolicy:
      return "ErrorCode::kOmahaUpdateIgnoredPerPolicy";
    case ErrorCode::kOmahaUpdateDeferredPerPolicy:
      return "ErrorCode::kOmahaUpdateDeferredPerPolicy";
    case ErrorCode::kOmahaErrorInHTTPResponse:
      return "ErrorCode::kOmahaErrorInHTTPResponse";
    case ErrorCode::kDownloadOperationHashMissingError:
      return "ErrorCode::kDownloadOperationHashMissingError";
    case ErrorCode::kDownloadMetadataSignatureMissingError:
      return "ErrorCode::kDownloadMetadataSignatureMissingError";
    case ErrorCode::kOmahaUpdateDeferredForBackoff:
      return "ErrorCode::kOmahaUpdateDeferredForBackoff";
    case ErrorCode::kPostinstallPowerwashError:
      return "ErrorCode::kPostinstallPowerwashError";
    case ErrorCode::kUpdateCanceledByChannelChange:
      return "ErrorCode::kUpdateCanceledByChannelChange";
    case ErrorCode::kUmaReportedMax:
      return "ErrorCode::kUmaReportedMax";
    case ErrorCode::kOmahaRequestHTTPResponseBase:
      return "ErrorCode::kOmahaRequestHTTPResponseBase";
    case ErrorCode::kResumedFlag:
      return "Resumed";
    case ErrorCode::kDevModeFlag:
      return "DevMode";
    case ErrorCode::kTestImageFlag:
      return "TestImage";
    case ErrorCode::kTestOmahaUrlFlag:
      return "TestOmahaUrl";
    case ErrorCode::kSpecialFlags:
      return "ErrorCode::kSpecialFlags";
    case ErrorCode::kPostinstallFirmwareRONotUpdatable:
      return "ErrorCode::kPostinstallFirmwareRONotUpdatable";
    case ErrorCode::kUnsupportedMajorPayloadVersion:
      return "ErrorCode::kUnsupportedMajorPayloadVersion";
    case ErrorCode::kUnsupportedMinorPayloadVersion:
      return "ErrorCode::kUnsupportedMinorPayloadVersion";
    case ErrorCode::kOmahaRequestXMLHasEntityDecl:
      return "ErrorCode::kOmahaRequestXMLHasEntityDecl";
    case ErrorCode::kFilesystemVerifierError:
      return "ErrorCode::kFilesystemVerifierError";
    // Don't add a default case to let the compiler warn about newly added
    // error codes which should be added here.
  }

  return "Unknown error: " + base::UintToString(static_cast<unsigned>(code));
}

bool CreatePowerwashMarkerFile(const char* file_path) {
  const char* marker_file = file_path ? file_path : kPowerwashMarkerFile;
  bool result = utils::WriteFile(marker_file,
                                 kPowerwashCommand,
                                 strlen(kPowerwashCommand));
  if (result) {
    LOG(INFO) << "Created " << marker_file << " to powerwash on next reboot";
  } else {
    PLOG(ERROR) << "Error in creating powerwash marker file: " << marker_file;
  }

  return result;
}

bool DeletePowerwashMarkerFile(const char* file_path) {
  const char* marker_file = file_path ? file_path : kPowerwashMarkerFile;
  const base::FilePath kPowerwashMarkerPath(marker_file);
  bool result = base::DeleteFile(kPowerwashMarkerPath, false);

  if (result)
    LOG(INFO) << "Successfully deleted the powerwash marker file : "
              << marker_file;
  else
    PLOG(ERROR) << "Could not delete the powerwash marker file : "
                << marker_file;

  return result;
}

bool GetInstallDev(const string& boot_dev, string* install_dev) {
  string disk_name;
  int partition_num;
  if (!SplitPartitionName(boot_dev, &disk_name, &partition_num))
    return false;

  // Right now, we just switch '3' and '5' partition numbers.
  if (partition_num == 3) {
    partition_num = 5;
  } else if (partition_num == 5) {
    partition_num = 3;
  } else {
    return false;
  }

  if (install_dev)
    *install_dev = MakePartitionName(disk_name, partition_num);

  return true;
}

Time TimeFromStructTimespec(struct timespec *ts) {
  int64_t us = static_cast<int64_t>(ts->tv_sec) * Time::kMicrosecondsPerSecond +
      static_cast<int64_t>(ts->tv_nsec) / Time::kNanosecondsPerMicrosecond;
  return Time::UnixEpoch() + TimeDelta::FromMicroseconds(us);
}

string StringVectorToString(const vector<string> &vec_str) {
  string str = "[";
  for (vector<string>::const_iterator i = vec_str.begin();
       i != vec_str.end(); ++i) {
    if (i != vec_str.begin())
      str += ", ";
    str += '"';
    str += *i;
    str += '"';
  }
  str += "]";
  return str;
}

string CalculateP2PFileId(const string& payload_hash, size_t payload_size) {
  string encoded_hash = chromeos::data_encoding::Base64Encode(payload_hash);
  return base::StringPrintf("cros_update_size_%zu_hash_%s",
                            payload_size,
                            encoded_hash.c_str());
}

bool DecodeAndStoreBase64String(const string& base64_encoded,
                                base::FilePath *out_path) {
  chromeos::Blob contents;

  out_path->clear();

  if (base64_encoded.size() == 0) {
    LOG(ERROR) << "Can't decode empty string.";
    return false;
  }

  if (!chromeos::data_encoding::Base64Decode(base64_encoded, &contents) ||
      contents.size() == 0) {
    LOG(ERROR) << "Error decoding base64.";
    return false;
  }

  FILE *file = base::CreateAndOpenTemporaryFile(out_path);
  if (file == nullptr) {
    LOG(ERROR) << "Error creating temporary file.";
    return false;
  }

  if (fwrite(contents.data(), 1, contents.size(), file) != contents.size()) {
    PLOG(ERROR) << "Error writing to temporary file.";
    if (fclose(file) != 0)
      PLOG(ERROR) << "Error closing temporary file.";
    if (unlink(out_path->value().c_str()) != 0)
      PLOG(ERROR) << "Error unlinking temporary file.";
    out_path->clear();
    return false;
  }

  if (fclose(file) != 0) {
    PLOG(ERROR) << "Error closing temporary file.";
    out_path->clear();
    return false;
  }

  return true;
}

bool ConvertToOmahaInstallDate(Time time, int *out_num_days) {
  time_t unix_time = time.ToTimeT();
  // Output of: date +"%s" --date="Jan 1, 2007 0:00 PST".
  const time_t kOmahaEpoch = 1167638400;
  const int64_t kNumSecondsPerWeek = 7*24*3600;
  const int64_t kNumDaysPerWeek = 7;

  time_t omaha_time = unix_time - kOmahaEpoch;

  if (omaha_time < 0)
    return false;

  // Note, as per the comment in utils.h we are deliberately not
  // handling DST correctly.

  int64_t num_weeks_since_omaha_epoch = omaha_time / kNumSecondsPerWeek;
  *out_num_days = num_weeks_since_omaha_epoch * kNumDaysPerWeek;

  return true;
}

bool WallclockDurationHelper(SystemState* system_state,
                             const string& state_variable_key,
                             TimeDelta* out_duration) {
  bool ret = false;

  Time now = system_state->clock()->GetWallclockTime();
  int64_t stored_value;
  if (system_state->prefs()->GetInt64(state_variable_key, &stored_value)) {
    Time stored_time = Time::FromInternalValue(stored_value);
    if (stored_time > now) {
      LOG(ERROR) << "Stored time-stamp used for " << state_variable_key
                 << " is in the future.";
    } else {
      *out_duration = now - stored_time;
      ret = true;
    }
  }

  if (!system_state->prefs()->SetInt64(state_variable_key,
                                       now.ToInternalValue())) {
    LOG(ERROR) << "Error storing time-stamp in " << state_variable_key;
  }

  return ret;
}

bool MonotonicDurationHelper(SystemState* system_state,
                             int64_t* storage,
                             TimeDelta* out_duration) {
  bool ret = false;

  Time now = system_state->clock()->GetMonotonicTime();
  if (*storage != 0) {
    Time stored_time = Time::FromInternalValue(*storage);
    *out_duration = now - stored_time;
    ret = true;
  }
  *storage = now.ToInternalValue();

  return ret;
}

bool GetMinorVersion(const chromeos::KeyValueStore& store,
                     uint32_t* minor_version) {
  string result;
  if (store.GetString("PAYLOAD_MINOR_VERSION", &result)) {
    if (!base::StringToUint(result, minor_version)) {
      LOG(ERROR) << "StringToUint failed when parsing delta minor version.";
      return false;
    }
    return true;
  }
  return false;
}

bool ReadExtents(const string& path, const vector<Extent>& extents,
                 chromeos::Blob* out_data, ssize_t out_data_size,
                 size_t block_size) {
  chromeos::Blob data(out_data_size);
  ssize_t bytes_read = 0;
  int fd = open(path.c_str(), O_RDONLY);
  TEST_AND_RETURN_FALSE_ERRNO(fd >= 0);
  ScopedFdCloser fd_closer(&fd);

  for (const Extent& extent : extents) {
    ssize_t bytes_read_this_iteration = 0;
    ssize_t bytes = extent.num_blocks() * block_size;
    TEST_AND_RETURN_FALSE(bytes_read + bytes <= out_data_size);
    TEST_AND_RETURN_FALSE(utils::PReadAll(fd,
                                          &data[bytes_read],
                                          bytes,
                                          extent.start_block() * block_size,
                                          &bytes_read_this_iteration));
    TEST_AND_RETURN_FALSE(bytes_read_this_iteration == bytes);
    bytes_read += bytes_read_this_iteration;
  }
  TEST_AND_RETURN_FALSE(out_data_size == bytes_read);
  *out_data = data;
  return true;
}

}  // namespace utils

}  // namespace chromeos_update_engine
