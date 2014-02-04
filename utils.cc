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

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/rand_util.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <glib.h>
#include <google/protobuf/stubs/common.h>

#include "update_engine/constants.h"
#include "update_engine/file_writer.h"
#include "update_engine/omaha_request_params.h"
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
}  // namespace

namespace utils {

// Cgroup container is created in update-engine's upstart script located at
// /etc/init/update-engine.conf.
static const char kCGroupDir[] = "/sys/fs/cgroup/cpu/update-engine";

string ParseECVersion(string input_line) {
  TrimWhitespaceASCII(input_line, TRIM_ALL, &input_line);

  // At this point we want to conver the format key=value pair from mosys to
  // a vector of key value pairs.
  vector<pair<string, string> > kv_pairs;
  if (base::SplitStringIntoKeyValuePairs(input_line, '=', ' ', &kv_pairs)) {
    for (vector<pair<string, string> >::iterator it = kv_pairs.begin();
         it != kv_pairs.end(); ++it) {
      // Finally match against the fw_verion which may have quotes.
      if (it->first == "fw_version") {
        string output;
        // Trim any quotes.
        TrimString(it->second, "\"", &output);
        return output;
      }
    }
  }
  LOG(ERROR) << "Unable to parse fwid from ec info.";
  return "";
}


const string KernelDeviceOfBootDevice(const std::string& boot_device) {
  if (boot_device.empty())
    return boot_device;

  string ubiblock_prefix("/dev/ubiblock");
  string ret;
  char partition_num;
  if (StringHasPrefix(boot_device, ubiblock_prefix)) {
    // eg: /dev/ubiblock3_0 becomes /dev/mtdblock2
    ret = "/dev/mtdblock";
    partition_num = boot_device[ubiblock_prefix.size()];
  } else {
    // eg: /dev/sda3 becomes /dev/sda2
    // eg: /dev/mmcblk0p3 becomes /dev/mmcblk0p2
    ret = boot_device.substr(0, boot_device.size() - 1);
    partition_num = boot_device[boot_device.size() - 1];
  }

  // Currently this assumes the partition number of the boot device is
  // 3, 5, or 7, and changes it to 2, 4, or 6, respectively, to
  // get the kernel device.
  if (partition_num == '3' || partition_num == '5' || partition_num == '7') {
    ret.append(1, partition_num - 1);
    return ret;
  }

  return "";
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
  file_util::ScopedFILE fp(fopen(path.c_str(), "r"));
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

string RootDevice(const string& partition_device) {
  FilePath device_path(partition_device);
  if (device_path.DirName().value() != "/dev") {
    return "";
  }
  string::const_iterator it = --partition_device.end();
  for (; it >= partition_device.begin(); --it) {
    if (!isdigit(*it))
      break;
  }
  // Some devices contain a p before the partitions. For example:
  // /dev/mmc0p4 should be shortened to /dev/mmc0.
  if (*it == 'p')
    --it;
  return string(partition_device.begin(), it + 1);
}

string PartitionNumber(const string& partition_device) {
  CHECK(!partition_device.empty());
  string::const_iterator it = --partition_device.end();
  for (; it >= partition_device.begin(); --it) {
    if (!isdigit(*it))
      break;
  }
  return string(it + 1, partition_device.end());
}

string SysfsBlockDevice(const string& device) {
  FilePath device_path(device);
  if (device_path.DirName().value() != "/dev") {
    return "";
  }
  return FilePath("/sys/block").Append(device_path.BaseName()).value();
}

bool IsRemovableDevice(const std::string& device) {
  string sysfs_block = SysfsBlockDevice(device);
  string removable;
  if (sysfs_block.empty() ||
      !file_util::ReadFileToString(FilePath(sysfs_block).Append("removable"),
                                   &removable)) {
    return false;
  }
  TrimWhitespaceASCII(removable, TRIM_ALL, &removable);
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
  return StringPrintf("%d/%d/%d %d:%02d:%02d GMT",
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
  const FilePath kPowerwashMarkerPath(marker_file);
  bool result = file_util::Delete(kPowerwashMarkerPath, false);

  if (result)
    LOG(INFO) << "Successfully deleted the powerwash marker file : "
              << marker_file;
  else
    PLOG(ERROR) << "Could not delete the powerwash marker file : "
                << marker_file;

  return result;
}

bool GetInstallDev(const std::string& boot_dev, std::string* install_dev) {
  TEST_AND_RETURN_FALSE(StringHasPrefix(boot_dev, "/dev/"));
  string::iterator it;
  string ubiblock_prefix("/dev/ubiblock");

  install_dev->assign(boot_dev);

  if(StringHasPrefix(boot_dev, ubiblock_prefix)) {
    // UBI-based device
    it = install_dev->begin() + ubiblock_prefix.length();
  } else {
    // non-UBI device
    it = install_dev->end() - 1;  // last character in string
  }

  // Right now, we just switch '3' and '5' partition numbers.
  TEST_AND_RETURN_FALSE(*it == '3' || *it == '5');
  *it = (*it == '3' ? '5' : '3');

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
  return StringPrintf("cros_update_size_%zu_hash_%s",
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

  FILE *file = file_util::CreateAndOpenTemporaryFile(out_path);
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

}  // namespace utils

}  // namespace chromeos_update_engine
