// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/utils.h"

#include <attr/xattr.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include <base/file_util.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <glib.h>
#include <google/protobuf/stubs/common.h>

#include "update_engine/clock_interface.h"
#include "update_engine/constants.h"
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

}  // namespace

namespace utils {

// Cgroup container is created in update-engine's upstart script located at
// /etc/init/update-engine.conf.
static const char kCGroupDir[] = "/sys/fs/cgroup/cpu/update-engine";

string ParseECVersion(string input_line) {
  base::TrimWhitespaceASCII(input_line, base::TRIM_ALL, &input_line);

  // At this point we want to convert the format key=value pair from mosys to
  // a vector of key value pairs.
  vector<pair<string, string> > kv_pairs;
  if (base::SplitStringIntoKeyValuePairs(input_line, '=', ' ', &kv_pairs)) {
    for (vector<pair<string, string> >::iterator it = kv_pairs.begin();
         it != kv_pairs.end(); ++it) {
      // Finally match against the fw_verion which may have quotes.
      if (it->first == "fw_version") {
        string output;
        // Trim any quotes.
        base::TrimString(it->second, "\"", &output);
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
    if (disk_name == "/dev/ubiblock") {
      // Special case for NAND devices.
      // eg: /dev/ubiblock3_0 becomes /dev/mtdblock2
      disk_name = "/dev/mtdblock";
    }
    // Currently this assumes the partition number of the boot device is
    // 3, 5, or 7, and changes it to 2, 4, or 6, respectively, to
    // get the kernel device.
    if (partition_num == 3 || partition_num == 5 || partition_num == 7) {
      kernel_partition_name = MakePartitionName(disk_name, partition_num - 1);
    }
  }

  return kernel_partition_name;
}


bool WriteFile(const char* path, const char* data, int data_len) {
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

// Append |nbytes| of content from |buf| to the vector pointed to by either
// |vec_p| or |str_p|.
static void AppendBytes(const char* buf, size_t nbytes,
                        std::vector<char>* vec_p) {
  CHECK(buf);
  CHECK(vec_p);
  vec_p->insert(vec_p->end(), buf, buf + nbytes);
}
static void AppendBytes(const char* buf, size_t nbytes,
                        std::string* str_p) {
  CHECK(buf);
  CHECK(str_p);
  str_p->append(buf, nbytes);
}

// Reads from an open file |fp|, appending the read content to the container
// pointer to by |out_p|.  Returns true upon successful reading all of the
// file's content, false otherwise. If |size| is not -1, reads up to |size|
// bytes.
template <class T>
static bool Read(FILE* fp, off_t size, T* out_p) {
  CHECK(fp);
  CHECK(size == -1 || size >= 0);
  char buf[1024];
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
static bool ReadFileChunkAndAppend(const std::string& path,
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

// Invokes a pipe |cmd|, then uses |append_func| to append its stdout to a
// container |out_p|.
template <class T>
static bool ReadPipeAndAppend(const std::string& cmd, T* out_p) {
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return false;
  bool success = Read(fp, -1, out_p);
  return (success && pclose(fp) >= 0);
}


bool ReadFile(const string& path, vector<char>* out_p) {
  return ReadFileChunkAndAppend(path, 0, -1, out_p);
}

bool ReadFile(const string& path, string* out_p) {
  return ReadFileChunkAndAppend(path, 0, -1, out_p);
}

bool ReadFileChunk(const string& path, off_t offset, off_t size,
                   vector<char>* out_p) {
  return ReadFileChunkAndAppend(path, offset, size, out_p);
}

bool ReadPipe(const string& cmd, vector<char>* out_p) {
  return ReadPipeAndAppend(cmd, out_p);
}

bool ReadPipe(const string& cmd, string* out_p) {
  return ReadPipeAndAppend(cmd, out_p);
}

off_t FileSize(const string& path) {
  struct stat stbuf;
  int rc = stat(path.c_str(), &stbuf);
  CHECK_EQ(rc, 0);
  if (rc < 0)
    return rc;
  return stbuf.st_size;
}

void HexDumpArray(const unsigned char* const arr, const size_t length) {
  const unsigned char* const char_arr =
      reinterpret_cast<const unsigned char* const>(arr);
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
      unsigned char c = char_arr[i + j];
      r = snprintf(buf, sizeof(buf), "%02x ", static_cast<unsigned int>(c));
      TEST_AND_RETURN(r == 3);
      line += buf;
    }
    LOG(INFO) << line;
  }
}

namespace {
class ScopedDirCloser {
 public:
  explicit ScopedDirCloser(DIR** dir) : dir_(dir) {}
  ~ScopedDirCloser() {
    if (dir_ && *dir_) {
      int r = closedir(*dir_);
      TEST_AND_RETURN_ERRNO(r == 0);
      *dir_ = NULL;
      dir_ = NULL;
    }
  }
 private:
  DIR** dir_;
};
}  // namespace {}

bool RecursiveUnlinkDir(const std::string& path) {
  struct stat stbuf;
  int r = lstat(path.c_str(), &stbuf);
  TEST_AND_RETURN_FALSE_ERRNO((r == 0) || (errno == ENOENT));
  if ((r < 0) && (errno == ENOENT))
    // path request is missing. that's fine.
    return true;
  if (!S_ISDIR(stbuf.st_mode)) {
    TEST_AND_RETURN_FALSE_ERRNO((unlink(path.c_str()) == 0) ||
                                (errno == ENOENT));
    // success or path disappeared before we could unlink.
    return true;
  }
  {
    // We have a dir, unlink all children, then delete dir
    DIR *dir = opendir(path.c_str());
    TEST_AND_RETURN_FALSE_ERRNO(dir);
    ScopedDirCloser dir_closer(&dir);
    struct dirent dir_entry;
    struct dirent *dir_entry_p;
    int err = 0;
    while ((err = readdir_r(dir, &dir_entry, &dir_entry_p)) == 0) {
      if (dir_entry_p == NULL) {
        // end of stream reached
        break;
      }
      // Skip . and ..
      if (!strcmp(dir_entry_p->d_name, ".") ||
          !strcmp(dir_entry_p->d_name, ".."))
        continue;
      TEST_AND_RETURN_FALSE(RecursiveUnlinkDir(path + "/" +
                                               dir_entry_p->d_name));
    }
    TEST_AND_RETURN_FALSE(err == 0);
  }
  // unlink dir
  TEST_AND_RETURN_FALSE_ERRNO((rmdir(path.c_str()) == 0) || (errno == ENOENT));
  return true;
}

std::string GetDiskName(const string& partition_name) {
  std::string disk_name;
  return SplitPartitionName(partition_name, &disk_name, nullptr) ?
         disk_name : std::string();
}

int GetPartitionNumber(const std::string& partition_name) {
  int partition_num = 0;
  return SplitPartitionName(partition_name, nullptr, &partition_num) ?
      partition_num : 0;
}

bool SplitPartitionName(const std::string& partition_name,
                        std::string* out_disk_name,
                        int* out_partition_num) {
  if (!StringHasPrefix(partition_name, "/dev/")) {
    LOG(ERROR) << "Invalid partition device name: " << partition_name;
    return false;
  }

  size_t last_nondigit_pos = partition_name.find_last_not_of("0123456789");
  if (last_nondigit_pos == string::npos ||
      (last_nondigit_pos + 1) == partition_name.size()) {
    LOG(ERROR) << "Unable to parse partition device name: " << partition_name;
    return false;
  }

  size_t partition_name_len = std::string::npos;
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
    std::string partition_str = partition_name.substr(last_nondigit_pos + 1,
                                                      partition_name_len);
    *out_partition_num = atoi(partition_str.c_str());
  }
  return true;
}

std::string MakePartitionName(const std::string& disk_name,
                              int partition_num) {
  if (!StringHasPrefix(disk_name, "/dev/")) {
    LOG(ERROR) << "Invalid disk name: " << disk_name;
    return std::string();
  }

  if (partition_num < 1) {
    LOG(ERROR) << "Invalid partition number: " << partition_num;
    return std::string();
  }

  std::string partition_name = disk_name;
  if (isdigit(partition_name.back())) {
    // Special case for devices with names ending with a digit.
    // Add "p" to separate the disk name from partition number,
    // e.g. "/dev/loop0p2"
    partition_name += 'p';
  }

  partition_name += std::to_string(partition_num);

  if (StringHasPrefix(partition_name, "/dev/ubiblock")) {
    // Special case for UBI block devieces that have "_0" suffix.
    partition_name += "_0";
  }
  return partition_name;
}

string SysfsBlockDevice(const string& device) {
  base::FilePath device_path(device);
  if (device_path.DirName().value() != "/dev") {
    return "";
  }
  return base::FilePath("/sys/block").Append(device_path.BaseName()).value();
}

bool IsRemovableDevice(const std::string& device) {
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

std::string ErrnoNumberAsString(int err) {
  char buf[100];
  buf[0] = '\0';
  return strerror_r(err, buf, sizeof(buf));
}

std::string NormalizePath(const std::string& path, bool strip_trailing_slash) {
  string ret;
  bool last_insert_was_slash = false;
  for (string::const_iterator it = path.begin(); it != path.end(); ++it) {
    if (*it == '/') {
      if (last_insert_was_slash)
        continue;
      last_insert_was_slash = true;
    } else {
      last_insert_was_slash = false;
    }
    ret.push_back(*it);
  }
  if (strip_trailing_slash && last_insert_was_slash) {
    string::size_type last_non_slash = ret.find_last_not_of('/');
    if (last_non_slash != string::npos) {
      ret.resize(last_non_slash + 1);
    } else {
      ret = "";
    }
  }
  return ret;
}

bool FileExists(const char* path) {
  struct stat stbuf;
  return 0 == lstat(path, &stbuf);
}

bool IsSymlink(const char* path) {
  struct stat stbuf;
  return lstat(path, &stbuf) == 0 && S_ISLNK(stbuf.st_mode) != 0;
}

bool IsDir(const char* path) {
  struct stat stbuf;
  TEST_AND_RETURN_FALSE_ERRNO(lstat(path, &stbuf) == 0);
  return S_ISDIR(stbuf.st_mode);
}

std::string TempFilename(string path) {
  static const string suffix("XXXXXX");
  CHECK(StringHasSuffix(path, suffix));
  do {
    string new_suffix;
    for (unsigned int i = 0; i < suffix.size(); i++) {
      int r = rand() % (26 * 2 + 10);  // [a-zA-Z0-9]
      if (r < 26)
        new_suffix.append(1, 'a' + r);
      else if (r < (26 * 2))
        new_suffix.append(1, 'A' + r - 26);
      else
        new_suffix.append(1, '0' + r - (26 * 2));
    }
    CHECK_EQ(new_suffix.size(), suffix.size());
    path.resize(path.size() - new_suffix.size());
    path.append(new_suffix);
  } while (FileExists(path.c_str()));
  return path;
}

// If |path| is absolute, or explicit relative to the current working directory,
// leaves it as is. Otherwise, if TMPDIR is defined in the environment and is
// non-empty, prepends it to |path|. Otherwise, prepends /tmp.  Returns the
// resulting path.
static const string PrependTmpdir(const string& path) {
  if (path[0] == '/' || StartsWithASCII(path, "./", true) ||
      StartsWithASCII(path, "../", true))
    return path;

  const char *tmpdir = getenv("TMPDIR");
  const string prefix = (tmpdir && *tmpdir ? tmpdir : "/tmp");
  return prefix + "/" + path;
}

bool MakeTempFile(const std::string& base_filename_template,
                  std::string* filename,
                  int* fd) {
  const string filename_template = PrependTmpdir(base_filename_template);
  DCHECK(filename || fd);
  vector<char> buf(filename_template.size() + 1);
  memcpy(&buf[0], filename_template.data(), filename_template.size());
  buf[filename_template.size()] = '\0';

  int mkstemp_fd = mkstemp(&buf[0]);
  TEST_AND_RETURN_FALSE_ERRNO(mkstemp_fd >= 0);
  if (filename) {
    *filename = &buf[0];
  }
  if (fd) {
    *fd = mkstemp_fd;
  } else {
    close(mkstemp_fd);
  }
  return true;
}

bool MakeTempDirectory(const std::string& base_dirname_template,
                       std::string* dirname) {
  const string dirname_template = PrependTmpdir(base_dirname_template);
  DCHECK(dirname);
  vector<char> buf(dirname_template.size() + 1);
  memcpy(&buf[0], dirname_template.data(), dirname_template.size());
  buf[dirname_template.size()] = '\0';

  char* return_code = mkdtemp(&buf[0]);
  TEST_AND_RETURN_FALSE_ERRNO(return_code != NULL);
  *dirname = &buf[0];
  return true;
}

bool StringHasSuffix(const std::string& str, const std::string& suffix) {
  if (suffix.size() > str.size())
    return false;
  return 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

bool StringHasPrefix(const std::string& str, const std::string& prefix) {
  if (prefix.size() > str.size())
    return false;
  return 0 == str.compare(0, prefix.size(), prefix);
}

bool MountFilesystem(const string& device,
                     const string& mountpoint,
                     unsigned long mountflags) {
  int rc = mount(device.c_str(), mountpoint.c_str(), "ext3", mountflags, NULL);
  if (rc < 0) {
    string msg = ErrnoNumberAsString(errno);
    LOG(ERROR) << "Unable to mount destination device: " << msg << ". "
               << device << " on " << mountpoint;
    return false;
  }
  return true;
}

bool UnmountFilesystem(const string& mountpoint) {
  for (int num_retries = 0; ; ++num_retries) {
    if (umount(mountpoint.c_str()) == 0)
      break;

    TEST_AND_RETURN_FALSE_ERRNO(errno == EBUSY &&
                                num_retries < kUnmountMaxNumOfRetries);
    g_usleep(kUnmountRetryIntervalInMicroseconds);
  }
  return true;
}

bool GetFilesystemSize(const std::string& device,
                       int* out_block_count,
                       int* out_block_size) {
  int fd = HANDLE_EINTR(open(device.c_str(), O_RDONLY));
  TEST_AND_RETURN_FALSE(fd >= 0);
  ScopedFdCloser fd_closer(&fd);
  return GetFilesystemSizeFromFD(fd, out_block_count, out_block_size);
}

bool GetFilesystemSizeFromFD(int fd,
                             int* out_block_count,
                             int* out_block_size) {
  TEST_AND_RETURN_FALSE(fd >= 0);

  // Determine the ext3 filesystem size by directly reading the block count and
  // block size information from the superblock. See include/linux/ext3_fs.h for
  // more details on the structure.
  ssize_t kBufferSize = 16 * sizeof(uint32_t);
  char buffer[kBufferSize];
  const int kSuperblockOffset = 1024;
  if (HANDLE_EINTR(pread(fd, buffer, kBufferSize, kSuperblockOffset)) !=
      kBufferSize) {
    PLOG(ERROR) << "Unable to determine file system size:";
    return false;
  }
  uint32_t block_count;  // ext3_fs.h: ext3_super_block.s_blocks_count
  uint32_t log_block_size;  // ext3_fs.h: ext3_super_block.s_log_block_size
  uint16_t magic;  // ext3_fs.h: ext3_super_block.s_magic
  memcpy(&block_count, &buffer[1 * sizeof(int32_t)], sizeof(block_count));
  memcpy(&log_block_size, &buffer[6 * sizeof(int32_t)], sizeof(log_block_size));
  memcpy(&magic, &buffer[14 * sizeof(int32_t)], sizeof(magic));
  block_count = le32toh(block_count);
  const int kExt3MinBlockLogSize = 10;  // ext3_fs.h: EXT3_MIN_BLOCK_LOG_SIZE
  log_block_size = le32toh(log_block_size) + kExt3MinBlockLogSize;
  magic = le16toh(magic);

  // Sanity check the parameters.
  const uint16_t kExt3SuperMagic = 0xef53;  // ext3_fs.h: EXT3_SUPER_MAGIC
  TEST_AND_RETURN_FALSE(magic == kExt3SuperMagic);
  const int kExt3MinBlockSize = 1024;  // ext3_fs.h: EXT3_MIN_BLOCK_SIZE
  const int kExt3MaxBlockSize = 4096;  // ext3_fs.h: EXT3_MAX_BLOCK_SIZE
  int block_size = 1 << log_block_size;
  TEST_AND_RETURN_FALSE(block_size >= kExt3MinBlockSize &&
                        block_size <= kExt3MaxBlockSize);
  TEST_AND_RETURN_FALSE(block_count > 0);

  if (out_block_count) {
    *out_block_count = block_count;
  }
  if (out_block_size) {
    *out_block_size = block_size;
  }
  return true;
}

// Tries to parse the header of an ELF file to obtain a human-readable
// description of it on the |output| string.
static bool GetFileFormatELF(const char* buffer, size_t size, string* output) {
  // 0x00: EI_MAG - ELF magic header, 4 bytes.
  if (size < 4 || memcmp(buffer, "\x7F""ELF", 4) != 0)
    return false;
  *output = "ELF";

  // 0x04: EI_CLASS, 1 byte.
  if (size < 0x04 + 1)
    return true;
  switch (buffer[4]) {
    case 1:
      *output += " 32-bit";
      break;
    case 2:
      *output += " 64-bit";
      break;
    default:
      *output += " ?-bit";
  }

  // 0x05: EI_DATA, endianness, 1 byte.
  if (size < 0x05 + 1)
    return true;
  char ei_data = buffer[5];
  switch (ei_data) {
    case 1:
      *output += " little-endian";
      break;
    case 2:
      *output += " big-endian";
      break;
    default:
      *output += " ?-endian";
      // Don't parse anything after the 0x10 offset if endianness is unknown.
      return true;
  }

  // 0x12: e_machine, 2 byte endianness based on ei_data
  if (size < 0x12 + 2)
    return true;
  uint16 e_machine = *reinterpret_cast<const uint16*>(buffer+0x12);
  // Fix endianess regardless of the host endianess.
  if (ei_data == 1)
    e_machine = le16toh(e_machine);
  else
    e_machine = be16toh(e_machine);

  switch (e_machine) {
    case 0x03:
      *output += " x86";
      break;
    case 0x28:
      *output += " arm";
      break;
    case 0x3E:
      *output += " x86-64";
      break;
    default:
      *output += " unknown-arch";
  }
  return true;
}

string GetFileFormat(const string& path) {
  vector<char> buffer;
  if (!ReadFileChunkAndAppend(path, 0, kGetFileFormatMaxHeaderSize, &buffer))
    return "File not found.";

  string result;
  if (GetFileFormatELF(buffer.data(), buffer.size(), &result))
    return result;

  return "data";
}

bool GetBootloader(BootLoader* out_bootloader) {
  // For now, hardcode to syslinux.
  *out_bootloader = BootLoader_SYSLINUX;
  return true;
}

string GetAndFreeGError(GError** error) {
  if (!*error) {
    return "Unknown GLib error.";
  }
  string message =
      base::StringPrintf("GError(%d): %s",
                         (*error)->code,
                         (*error)->message ? (*error)->message : "(unknown)");
  g_error_free(*error);
  *error = NULL;
  return message;
}

bool Reboot() {
  vector<string> command;
  command.push_back("/sbin/shutdown");
  command.push_back("-r");
  command.push_back("now");
  int rc = 0;
  Subprocess::SynchronousExec(command, &rc, NULL);
  TEST_AND_RETURN_FALSE(rc == 0);
  return true;
}

namespace {
// Do the actual trigger. We do it as a main-loop callback to (try to) get a
// consistent stack trace.
gboolean TriggerCrashReporterUpload(void* unused) {
  pid_t pid = fork();
  CHECK(pid >= 0) << "fork failed";  // fork() failed. Something is very wrong.
  if (pid == 0) {
    // We are the child. Crash.
    abort();  // never returns
  }
  // We are the parent. Wait for child to terminate.
  pid_t result = waitpid(pid, NULL, 0);
  LOG_IF(ERROR, result < 0) << "waitpid() failed";
  return FALSE;  // Don't call this callback again
}
}  // namespace {}

void ScheduleCrashReporterUpload() {
  g_idle_add(&TriggerCrashReporterUpload, NULL);
}

bool SetCpuShares(CpuShares shares) {
  string string_shares = base::IntToString(static_cast<int>(shares));
  string cpu_shares_file = string(utils::kCGroupDir) + "/cpu.shares";
  LOG(INFO) << "Setting cgroup cpu shares to  " << string_shares;
  if(utils::WriteFile(cpu_shares_file.c_str(), string_shares.c_str(),
                      string_shares.size())){
    return true;
  } else {
    LOG(ERROR) << "Failed to change cgroup cpu shares to "<< string_shares
               << " using " << cpu_shares_file;
    return false;
  }
}

int CompareCpuShares(CpuShares shares_lhs,
                     CpuShares shares_rhs) {
  return static_cast<int>(shares_lhs) - static_cast<int>(shares_rhs);
}

int FuzzInt(int value, unsigned int range) {
  int min = value - range / 2;
  int max = value + range - range / 2;
  return base::RandInt(min, max);
}

gboolean GlibRunClosure(gpointer data) {
  google::protobuf::Closure* callback =
      reinterpret_cast<google::protobuf::Closure*>(data);
  callback->Run();
  return FALSE;
}

string FormatSecs(unsigned secs) {
  return FormatTimeDelta(TimeDelta::FromSeconds(secs));
}

string FormatTimeDelta(TimeDelta delta) {
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

  // Construct and return string.
  string str;
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
  // with values less than kErrorCodeUmaReportedMax.
  ErrorCode base_code = static_cast<ErrorCode>(code & ~kErrorCodeSpecialFlags);

  // Make additional adjustments required for UMA and error classification.
  // TODO(jaysri): Move this logic to UeErrorCode.cc when we fix
  // chromium-os:34369.
  if (base_code >= kErrorCodeOmahaRequestHTTPResponseBase) {
    // Since we want to keep the enums to a small value, aggregate all HTTP
    // errors into this one bucket for UMA and error classification purposes.
    LOG(INFO) << "Converting error code " << base_code
              << " to kErrorCodeOmahaErrorInHTTPResponse";
    base_code = kErrorCodeOmahaErrorInHTTPResponse;
  }

  return base_code;
}

metrics::AttemptResult GetAttemptResult(ErrorCode code) {
  ErrorCode base_code = static_cast<ErrorCode>(code & ~kErrorCodeSpecialFlags);

  switch (base_code) {
    case kErrorCodeSuccess:
      return metrics::AttemptResult::kUpdateSucceeded;

    case kErrorCodeDownloadTransferError:
      return metrics::AttemptResult::kPayloadDownloadError;

    case kErrorCodeDownloadInvalidMetadataSize:
    case kErrorCodeDownloadInvalidMetadataMagicString:
    case kErrorCodeDownloadMetadataSignatureError:
    case kErrorCodeDownloadMetadataSignatureVerificationError:
    case kErrorCodePayloadMismatchedType:
    case kErrorCodeUnsupportedMajorPayloadVersion:
    case kErrorCodeUnsupportedMinorPayloadVersion:
    case kErrorCodeDownloadNewPartitionInfoError:
    case kErrorCodeDownloadSignatureMissingInManifest:
    case kErrorCodeDownloadManifestParseError:
    case kErrorCodeDownloadOperationHashMissingError:
      return metrics::AttemptResult::kMetadataMalformed;

    case kErrorCodeDownloadOperationHashMismatch:
    case kErrorCodeDownloadOperationHashVerificationError:
      return metrics::AttemptResult::kOperationMalformed;

    case kErrorCodeDownloadOperationExecutionError:
    case kErrorCodeInstallDeviceOpenError:
    case kErrorCodeKernelDeviceOpenError:
    case kErrorCodeDownloadWriteError:
    case kErrorCodeFilesystemCopierError:
      return metrics::AttemptResult::kOperationExecutionError;

    case kErrorCodeDownloadMetadataSignatureMismatch:
      return metrics::AttemptResult::kMetadataVerificationFailed;

    case kErrorCodePayloadSizeMismatchError:
    case kErrorCodePayloadHashMismatchError:
    case kErrorCodeDownloadPayloadVerificationError:
    case kErrorCodeSignedDeltaPayloadExpectedError:
    case kErrorCodeDownloadPayloadPubKeyVerificationError:
      return metrics::AttemptResult::kPayloadVerificationFailed;

    case kErrorCodeNewRootfsVerificationError:
    case kErrorCodeNewKernelVerificationError:
      return metrics::AttemptResult::kVerificationFailed;

    case kErrorCodePostinstallRunnerError:
    case kErrorCodePostinstallBootedFromFirmwareB:
    case kErrorCodePostinstallFirmwareRONotUpdatable:
      return metrics::AttemptResult::kPostInstallFailed;

    // We should never get these errors in the update-attempt stage so
    // return internal error if this happens.
    case kErrorCodeError:
    case kErrorCodeOmahaRequestXMLParseError:
    case kErrorCodeOmahaRequestError:
    case kErrorCodeOmahaResponseHandlerError:
    case kErrorCodeDownloadStateInitializationError:
    case kErrorCodeOmahaRequestEmptyResponseError:
    case kErrorCodeDownloadInvalidMetadataSignature:
    case kErrorCodeOmahaResponseInvalid:
    case kErrorCodeOmahaUpdateIgnoredPerPolicy:
    case kErrorCodeOmahaUpdateDeferredPerPolicy:
    case kErrorCodeOmahaErrorInHTTPResponse:
    case kErrorCodeDownloadMetadataSignatureMissingError:
    case kErrorCodeOmahaUpdateDeferredForBackoff:
    case kErrorCodePostinstallPowerwashError:
    case kErrorCodeUpdateCanceledByChannelChange:
      return metrics::AttemptResult::kInternalError;

    // Special flags. These can't happen (we mask them out above) but
    // the compiler doesn't know that. Just break out so we can warn and
    // return |kInternalError|.
    case kErrorCodeUmaReportedMax:
    case kErrorCodeOmahaRequestHTTPResponseBase:
    case kErrorCodeDevModeFlag:
    case kErrorCodeResumedFlag:
    case kErrorCodeTestImageFlag:
    case kErrorCodeTestOmahaUrlFlag:
    case kErrorCodeSpecialFlags:
      break;
  }

  LOG(ERROR) << "Unexpected error code " << base_code;
  return metrics::AttemptResult::kInternalError;
}


metrics::DownloadErrorCode GetDownloadErrorCode(ErrorCode code) {
  ErrorCode base_code = static_cast<ErrorCode>(code & ~kErrorCodeSpecialFlags);

  if (base_code >= kErrorCodeOmahaRequestHTTPResponseBase) {
    int http_status = base_code - kErrorCodeOmahaRequestHTTPResponseBase;
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
    // Unfortunately, kErrorCodeDownloadTransferError is returned for a wide
    // variety of errors (proxy errors, host not reachable, timeouts etc.).
    //
    // For now just map that to kDownloading. See http://crbug.com/355745
    // for how we plan to add more detail in the future.
    case kErrorCodeDownloadTransferError:
      return metrics::DownloadErrorCode::kDownloadError;

    // All of these error codes are not related to downloading so break
    // out so we can warn and return InputMalformed.
    case kErrorCodeSuccess:
    case kErrorCodeError:
    case kErrorCodeOmahaRequestError:
    case kErrorCodeOmahaResponseHandlerError:
    case kErrorCodeFilesystemCopierError:
    case kErrorCodePostinstallRunnerError:
    case kErrorCodePayloadMismatchedType:
    case kErrorCodeInstallDeviceOpenError:
    case kErrorCodeKernelDeviceOpenError:
    case kErrorCodePayloadHashMismatchError:
    case kErrorCodePayloadSizeMismatchError:
    case kErrorCodeDownloadPayloadVerificationError:
    case kErrorCodeDownloadNewPartitionInfoError:
    case kErrorCodeDownloadWriteError:
    case kErrorCodeNewRootfsVerificationError:
    case kErrorCodeNewKernelVerificationError:
    case kErrorCodeSignedDeltaPayloadExpectedError:
    case kErrorCodeDownloadPayloadPubKeyVerificationError:
    case kErrorCodePostinstallBootedFromFirmwareB:
    case kErrorCodeDownloadStateInitializationError:
    case kErrorCodeDownloadInvalidMetadataMagicString:
    case kErrorCodeDownloadSignatureMissingInManifest:
    case kErrorCodeDownloadManifestParseError:
    case kErrorCodeDownloadMetadataSignatureError:
    case kErrorCodeDownloadMetadataSignatureVerificationError:
    case kErrorCodeDownloadMetadataSignatureMismatch:
    case kErrorCodeDownloadOperationHashVerificationError:
    case kErrorCodeDownloadOperationExecutionError:
    case kErrorCodeDownloadOperationHashMismatch:
    case kErrorCodeOmahaRequestEmptyResponseError:
    case kErrorCodeOmahaRequestXMLParseError:
    case kErrorCodeDownloadInvalidMetadataSize:
    case kErrorCodeDownloadInvalidMetadataSignature:
    case kErrorCodeOmahaResponseInvalid:
    case kErrorCodeOmahaUpdateIgnoredPerPolicy:
    case kErrorCodeOmahaUpdateDeferredPerPolicy:
    case kErrorCodeOmahaErrorInHTTPResponse:
    case kErrorCodeDownloadOperationHashMissingError:
    case kErrorCodeDownloadMetadataSignatureMissingError:
    case kErrorCodeOmahaUpdateDeferredForBackoff:
    case kErrorCodePostinstallPowerwashError:
    case kErrorCodeUpdateCanceledByChannelChange:
    case kErrorCodePostinstallFirmwareRONotUpdatable:
    case kErrorCodeUnsupportedMajorPayloadVersion:
    case kErrorCodeUnsupportedMinorPayloadVersion:
      break;

    // Special flags. These can't happen (we mask them out above) but
    // the compiler doesn't know that. Just break out so we can warn and
    // return |kInputMalformed|.
    case kErrorCodeUmaReportedMax:
    case kErrorCodeOmahaRequestHTTPResponseBase:
    case kErrorCodeDevModeFlag:
    case kErrorCodeResumedFlag:
    case kErrorCodeTestImageFlag:
    case kErrorCodeTestOmahaUrlFlag:
    case kErrorCodeSpecialFlags:
      LOG(ERROR) << "Unexpected error code " << base_code;
      break;
  }

  return metrics::DownloadErrorCode::kInputMalformed;
}

metrics::ConnectionType GetConnectionType(
    NetworkConnectionType type,
    NetworkTethering tethering) {
  switch (type) {
    case kNetUnknown:
      return metrics::ConnectionType::kUnknown;

    case kNetEthernet:
      if (tethering == NetworkTethering::kConfirmed)
        return metrics::ConnectionType::kTetheredEthernet;
      else
        return metrics::ConnectionType::kEthernet;

    case kNetWifi:
      if (tethering == NetworkTethering::kConfirmed)
        return metrics::ConnectionType::kTetheredWifi;
      else
        return metrics::ConnectionType::kWifi;

    case kNetWimax:
      return metrics::ConnectionType::kWimax;

    case kNetBluetooth:
      return metrics::ConnectionType::kBluetooth;

    case kNetCellular:
      return metrics::ConnectionType::kCellular;
  }

  LOG(ERROR) << "Unexpected network connection type: type=" << type
             << ", tethering=" << static_cast<int>(tethering);

  return metrics::ConnectionType::kUnknown;
}

// Returns a printable version of the various flags denoted in the higher order
// bits of the given code. Returns an empty string if none of those bits are
// set.
string GetFlagNames(uint32_t code) {
  uint32_t flags = code & kErrorCodeSpecialFlags;
  string flag_names;
  string separator = "";
  for(size_t i = 0; i < sizeof(flags) * 8; i++) {
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
  uint32_t flags = code & kErrorCodeSpecialFlags;
  if (!flags)
    flags = system_state->update_attempter()->GetErrorCodeFlags();

  // Determine the UMA bucket depending on the flags. But, ignore the resumed
  // flag, as it's perfectly normal for production devices to resume their
  // downloads and so we want to record those cases also in NormalErrorCodes
  // bucket.
  string metric = (flags & ~kErrorCodeResumedFlag) ?
      "Installer.DevModeErrorCodes" : "Installer.NormalErrorCodes";

  LOG(INFO) << "Sending error code " << uma_error_code
            << " (" << CodeToString(uma_error_code) << ")"
            << " to UMA metric: " << metric
            << ". Flags = " << (flags ? GetFlagNames(flags) : "None");

  system_state->metrics_lib()->SendEnumToUMA(metric,
                                             uma_error_code,
                                             kErrorCodeUmaReportedMax);
}

string CodeToString(ErrorCode code) {
  // If the given code has both parts (i.e. the error code part and the flags
  // part) then strip off the flags part since the switch statement below
  // has case statements only for the base error code or a single flag but
  // doesn't support any combinations of those.
  if ((code & kErrorCodeSpecialFlags) && (code & ~kErrorCodeSpecialFlags))
    code = static_cast<ErrorCode>(code & ~kErrorCodeSpecialFlags);
  switch (code) {
    case kErrorCodeSuccess: return "kErrorCodeSuccess";
    case kErrorCodeError: return "kErrorCodeError";
    case kErrorCodeOmahaRequestError: return "kErrorCodeOmahaRequestError";
    case kErrorCodeOmahaResponseHandlerError:
      return "kErrorCodeOmahaResponseHandlerError";
    case kErrorCodeFilesystemCopierError:
      return "kErrorCodeFilesystemCopierError";
    case kErrorCodePostinstallRunnerError:
      return "kErrorCodePostinstallRunnerError";
    case kErrorCodePayloadMismatchedType:
      return "kErrorCodePayloadMismatchedType";
    case kErrorCodeInstallDeviceOpenError:
      return "kErrorCodeInstallDeviceOpenError";
    case kErrorCodeKernelDeviceOpenError:
      return "kErrorCodeKernelDeviceOpenError";
    case kErrorCodeDownloadTransferError:
      return "kErrorCodeDownloadTransferError";
    case kErrorCodePayloadHashMismatchError:
      return "kErrorCodePayloadHashMismatchError";
    case kErrorCodePayloadSizeMismatchError:
      return "kErrorCodePayloadSizeMismatchError";
    case kErrorCodeDownloadPayloadVerificationError:
      return "kErrorCodeDownloadPayloadVerificationError";
    case kErrorCodeDownloadNewPartitionInfoError:
      return "kErrorCodeDownloadNewPartitionInfoError";
    case kErrorCodeDownloadWriteError:
      return "kErrorCodeDownloadWriteError";
    case kErrorCodeNewRootfsVerificationError:
      return "kErrorCodeNewRootfsVerificationError";
    case kErrorCodeNewKernelVerificationError:
      return "kErrorCodeNewKernelVerificationError";
    case kErrorCodeSignedDeltaPayloadExpectedError:
      return "kErrorCodeSignedDeltaPayloadExpectedError";
    case kErrorCodeDownloadPayloadPubKeyVerificationError:
      return "kErrorCodeDownloadPayloadPubKeyVerificationError";
    case kErrorCodePostinstallBootedFromFirmwareB:
      return "kErrorCodePostinstallBootedFromFirmwareB";
    case kErrorCodeDownloadStateInitializationError:
      return "kErrorCodeDownloadStateInitializationError";
    case kErrorCodeDownloadInvalidMetadataMagicString:
      return "kErrorCodeDownloadInvalidMetadataMagicString";
    case kErrorCodeDownloadSignatureMissingInManifest:
      return "kErrorCodeDownloadSignatureMissingInManifest";
    case kErrorCodeDownloadManifestParseError:
      return "kErrorCodeDownloadManifestParseError";
    case kErrorCodeDownloadMetadataSignatureError:
      return "kErrorCodeDownloadMetadataSignatureError";
    case kErrorCodeDownloadMetadataSignatureVerificationError:
      return "kErrorCodeDownloadMetadataSignatureVerificationError";
    case kErrorCodeDownloadMetadataSignatureMismatch:
      return "kErrorCodeDownloadMetadataSignatureMismatch";
    case kErrorCodeDownloadOperationHashVerificationError:
      return "kErrorCodeDownloadOperationHashVerificationError";
    case kErrorCodeDownloadOperationExecutionError:
      return "kErrorCodeDownloadOperationExecutionError";
    case kErrorCodeDownloadOperationHashMismatch:
      return "kErrorCodeDownloadOperationHashMismatch";
    case kErrorCodeOmahaRequestEmptyResponseError:
      return "kErrorCodeOmahaRequestEmptyResponseError";
    case kErrorCodeOmahaRequestXMLParseError:
      return "kErrorCodeOmahaRequestXMLParseError";
    case kErrorCodeDownloadInvalidMetadataSize:
      return "kErrorCodeDownloadInvalidMetadataSize";
    case kErrorCodeDownloadInvalidMetadataSignature:
      return "kErrorCodeDownloadInvalidMetadataSignature";
    case kErrorCodeOmahaResponseInvalid:
      return "kErrorCodeOmahaResponseInvalid";
    case kErrorCodeOmahaUpdateIgnoredPerPolicy:
      return "kErrorCodeOmahaUpdateIgnoredPerPolicy";
    case kErrorCodeOmahaUpdateDeferredPerPolicy:
      return "kErrorCodeOmahaUpdateDeferredPerPolicy";
    case kErrorCodeOmahaErrorInHTTPResponse:
      return "kErrorCodeOmahaErrorInHTTPResponse";
    case kErrorCodeDownloadOperationHashMissingError:
      return "kErrorCodeDownloadOperationHashMissingError";
    case kErrorCodeDownloadMetadataSignatureMissingError:
      return "kErrorCodeDownloadMetadataSignatureMissingError";
    case kErrorCodeOmahaUpdateDeferredForBackoff:
      return "kErrorCodeOmahaUpdateDeferredForBackoff";
    case kErrorCodePostinstallPowerwashError:
      return "kErrorCodePostinstallPowerwashError";
    case kErrorCodeUpdateCanceledByChannelChange:
      return "kErrorCodeUpdateCanceledByChannelChange";
    case kErrorCodeUmaReportedMax:
      return "kErrorCodeUmaReportedMax";
    case kErrorCodeOmahaRequestHTTPResponseBase:
      return "kErrorCodeOmahaRequestHTTPResponseBase";
    case kErrorCodeResumedFlag:
      return "Resumed";
    case kErrorCodeDevModeFlag:
      return "DevMode";
    case kErrorCodeTestImageFlag:
      return "TestImage";
    case kErrorCodeTestOmahaUrlFlag:
      return "TestOmahaUrl";
    case kErrorCodeSpecialFlags:
      return "kErrorCodeSpecialFlags";
    case kErrorCodePostinstallFirmwareRONotUpdatable:
      return "kErrorCodePostinstallFirmwareRONotUpdatable";
    case kErrorCodeUnsupportedMajorPayloadVersion:
      return "kErrorCodeUnsupportedMajorPayloadVersion";
    case kErrorCodeUnsupportedMinorPayloadVersion:
      return "kErrorCodeUnsupportedMinorPayloadVersion";
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

bool GetInstallDev(const std::string& boot_dev, std::string* install_dev) {
  std::string disk_name;
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
  int64 us = static_cast<int64>(ts->tv_sec) * Time::kMicrosecondsPerSecond +
      static_cast<int64>(ts->tv_nsec) / Time::kNanosecondsPerMicrosecond;
  return Time::UnixEpoch() + TimeDelta::FromMicroseconds(us);
}

gchar** StringVectorToGStrv(const vector<string> &vector) {
  GPtrArray *p = g_ptr_array_new();
  for (std::vector<string>::const_iterator i = vector.begin();
       i != vector.end(); ++i) {
    g_ptr_array_add(p, g_strdup(i->c_str()));
  }
  g_ptr_array_add(p, NULL);
  return reinterpret_cast<gchar**>(g_ptr_array_free(p, FALSE));
}

string StringVectorToString(const vector<string> &vector) {
  string str = "[";
  for (std::vector<string>::const_iterator i = vector.begin();
       i != vector.end(); ++i) {
    if (i != vector.begin())
      str += ", ";
    str += '"';
    str += *i;
    str += '"';
  }
  str += "]";
  return str;
}

string CalculateP2PFileId(const string& payload_hash, size_t payload_size) {
  string encoded_hash;
  OmahaHashCalculator::Base64Encode(payload_hash.c_str(),
                                    payload_hash.size(),
                                    &encoded_hash);
  return base::StringPrintf("cros_update_size_%zu_hash_%s",
                      payload_size,
                      encoded_hash.c_str());
}

bool IsXAttrSupported(const base::FilePath& dir_path) {
  char *path = strdup(dir_path.Append("xattr_test_XXXXXX").value().c_str());

  int fd = mkstemp(path);
  if (fd == -1) {
    PLOG(ERROR) << "Error creating temporary file in " << dir_path.value();
    free(path);
    return false;
  }

  if (unlink(path) != 0) {
    PLOG(ERROR) << "Error unlinking temporary file " << path;
    close(fd);
    free(path);
    return false;
  }

  int xattr_res = fsetxattr(fd, "user.xattr-test", "value", strlen("value"), 0);
  if (xattr_res != 0) {
    if (errno == ENOTSUP) {
      // Leave it to call-sites to warn about non-support.
    } else {
      PLOG(ERROR) << "Error setting xattr on " << path;
    }
  }
  close(fd);
  free(path);
  return xattr_res == 0;
}

bool DecodeAndStoreBase64String(const std::string& base64_encoded,
                                base::FilePath *out_path) {
  vector<char> contents;

  out_path->clear();

  if (base64_encoded.size() == 0) {
    LOG(ERROR) << "Can't decode empty string.";
    return false;
  }

  if (!OmahaHashCalculator::Base64Decode(base64_encoded, &contents) ||
      contents.size() == 0) {
    LOG(ERROR) << "Error decoding base64.";
    return false;
  }

  FILE *file = base::CreateAndOpenTemporaryFile(out_path);
  if (file == NULL) {
    LOG(ERROR) << "Error creating temporary file.";
    return false;
  }

  if (fwrite(&contents[0], 1, contents.size(), file) != contents.size()) {
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

bool ConvertToOmahaInstallDate(base::Time time, int *out_num_days) {
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
                             const std::string& state_variable_key,
                             base::TimeDelta* out_duration) {
  bool ret = false;

  base::Time now = system_state->clock()->GetWallclockTime();
  int64_t stored_value;
  if (system_state->prefs()->GetInt64(state_variable_key, &stored_value)) {
    base::Time stored_time = base::Time::FromInternalValue(stored_value);
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
                             base::TimeDelta* out_duration) {
  bool ret = false;

  base::Time now = system_state->clock()->GetMonotonicTime();
  if (*storage != 0) {
    base::Time stored_time = base::Time::FromInternalValue(*storage);
    *out_duration = now - stored_time;
    ret = true;
  }
  *storage = now.ToInternalValue();

  return ret;
}

}  // namespace utils

}  // namespace chromeos_update_engine
