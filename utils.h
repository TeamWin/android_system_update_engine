// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UTILS_H_
#define UPDATE_ENGINE_UTILS_H_

#include <errno.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/posix/eintr_wrapper.h>
#include <base/time/time.h>
#include <glib.h>
#include "metrics/metrics_library.h"

#include "update_engine/action.h"
#include "update_engine/action_processor.h"
#include "update_engine/connection_manager.h"
#include "update_engine/constants.h"
#include "update_engine/metrics.h"

namespace chromeos_update_engine {

class SystemState;

namespace utils {

// Converts a struct timespec representing a number of seconds since
// the Unix epoch to a base::Time. Sub-microsecond time is rounded
// down.
base::Time TimeFromStructTimespec(struct timespec *ts);

// Converts a vector of strings to a NUL-terminated array of
// strings. The resulting array should be freed with g_strfreev()
// when are you done with it.
gchar** StringVectorToGStrv(const std::vector<std::string> &vec_str);

// Formats |vec_str| as a string of the form ["<elem1>", "<elem2>"].
// Does no escaping, only use this for presentation in error messages.
std::string StringVectorToString(const std::vector<std::string> &vec_str);

// Calculates the p2p file id from payload hash and size
std::string CalculateP2PFileId(const std::string& payload_hash,
                               size_t payload_size);

// Parse the firmware version from one line of output from the
// "mosys" command.
std::string ParseECVersion(std::string input_line);

// Given the name of the block device of a boot partition, return the
// name of the associated kernel partition (e.g. given "/dev/sda3",
// return "/dev/sda2").
const std::string KernelDeviceOfBootDevice(const std::string& boot_device);

// Writes the data passed to path. The file at path will be overwritten if it
// exists. Returns true on success, false otherwise.
bool WriteFile(const char* path, const char* data, int data_len);

// Calls write() or pwrite() repeatedly until all count bytes at buf are
// written to fd or an error occurs. Returns true on success.
bool WriteAll(int fd, const void* buf, size_t count);
bool PWriteAll(int fd, const void* buf, size_t count, off_t offset);

// Calls pread() repeatedly until count bytes are read, or EOF is reached.
// Returns number of bytes read in *bytes_read. Returns true on success.
bool PReadAll(int fd, void* buf, size_t count, off_t offset,
              ssize_t* out_bytes_read);

// Opens |path| for reading and appends its entire content to the container
// pointed to by |out_p|. Returns true upon successfully reading all of the
// file's content, false otherwise, in which case the state of the output
// container is unknown. ReadFileChunk starts reading the file from |offset|; if
// |size| is not -1, only up to |size| bytes are read in.
bool ReadFile(const std::string& path, std::vector<char>* out_p);
bool ReadFile(const std::string& path, std::string* out_p);
bool ReadFileChunk(const std::string& path, off_t offset, off_t size,
                   std::vector<char>* out_p);

// Invokes |cmd| in a pipe and appends its stdout to the container pointed to by
// |out_p|. Returns true upon successfully reading all of the output, false
// otherwise, in which case the state of the output container is unknown.
bool ReadPipe(const std::string& cmd, std::vector<char>* out_p);
bool ReadPipe(const std::string& cmd, std::string* out_p);

// Returns the size of the block device at path, or the file descriptor fd. If
// an error occurs, -1 is returned.
off_t BlockDevSize(const std::string& path);
off_t BlockDevSize(int fd);

// Returns the size of the file at path, or the file desciptor fd. If the file
// is actually a block device, this function will automatically call
// BlockDevSize. If the file doesn't exist or some error occurrs, -1 is
// returned.
off_t FileSize(const std::string& path);
off_t FileSize(int fd);

std::string ErrnoNumberAsString(int err);

// Strips duplicate slashes, and optionally removes all trailing slashes.
// Does not compact /./ or /../.
std::string NormalizePath(const std::string& path, bool strip_trailing_slash);

// Returns true if the file exists for sure. Returns false if it doesn't exist,
// or an error occurs.
bool FileExists(const char* path);

// Returns true if |path| exists and is a symbolic link.
bool IsSymlink(const char* path);

// Returns true if |path| exists and is a directory.
bool IsDir(const char* path);

// If |base_filename_template| is neither absolute (starts with "/") nor
// explicitly relative to the current working directory (starts with "./" or
// "../"), then it is prepended the value of TMPDIR, which defaults to /tmp if
// it isn't set or is empty.  It then calls mkstemp(3) with the resulting
// template.  Writes the name of a new temporary file to |filename|. If |fd| is
// non-null, the file descriptor returned by mkstemp is written to it and
// kept open; otherwise, it is closed. The template must end with "XXXXXX".
// Returns true on success.
bool MakeTempFile(const std::string& base_filename_template,
                  std::string* filename,
                  int* fd);

// If |base_filename_template| is neither absolute (starts with "/") nor
// explicitly relative to the current working directory (starts with "./" or
// "../"), then it is prepended the value of TMPDIR, which defaults to /tmp if
// it isn't set or is empty.  It then calls mkdtemp() with the resulting
// template. Writes the name of the new temporary directory to |dirname|.
// The template must end with "XXXXXX". Returns true on success.
bool MakeTempDirectory(const std::string& base_dirname_template,
                       std::string* dirname);

// Deletes a directory and all its contents synchronously. Returns true
// on success. This may be called with a regular file--it will just unlink it.
// This WILL cross filesystem boundaries.
bool RecursiveUnlinkDir(const std::string& path);

// Returns the disk device name for a partition. For example,
// GetDiskName("/dev/sda3") returns "/dev/sda". Returns an empty string
// if the input device is not of the "/dev/xyz#" form.
std::string GetDiskName(const std::string& partition_name);

// Returns the partition number, of partition device name. For example,
// GetPartitionNumber("/dev/sda3") returns 3.
// Returns 0 on failure
int GetPartitionNumber(const std::string& partition_name);

// Splits the partition device name into the block device name and partition
// number. For example, "/dev/sda3" will be split into {"/dev/sda", 3} and
// "/dev/mmcblk0p2" into {"/dev/mmcblk0", 2}
// Returns false when malformed device name is passed in.
// If both output parameters are omitted (null), can be used
// just to test the validity of the device name. Note that the function
// simply checks if the device name looks like a valid device, no other
// checks are performed (i.e. it doesn't check if the device actually exists).
bool SplitPartitionName(const std::string& partition_name,
                        std::string* out_disk_name,
                        int* out_partition_num);

// Builds a partition device name from the block device name and partition
// number. For example:
// {"/dev/sda", 1} => "/dev/sda1"
// {"/dev/mmcblk2", 12} => "/dev/mmcblk2p12"
// Returns empty string when invalid parameters are passed in
std::string MakePartitionName(const std::string& disk_name,
                              int partition_num);

// Returns the sysfs block device for a root block device. For
// example, SysfsBlockDevice("/dev/sda") returns
// "/sys/block/sda". Returns an empty string if the input device is
// not of the "/dev/xyz" form.
std::string SysfsBlockDevice(const std::string& device);

// Returns true if the root |device| (e.g., "/dev/sdb") is known to be
// removable, false otherwise.
bool IsRemovableDevice(const std::string& device);

// Synchronously mount or unmount a filesystem. Return true on success.
// Mounts as ext3 with default options.
bool MountFilesystem(const std::string& device, const std::string& mountpoint,
                     unsigned long flags);  // NOLINT(runtime/int)
bool UnmountFilesystem(const std::string& mountpoint);

// Returns the block count and the block byte size of the ext3 file system on
// |device| (which may be a real device or a path to a filesystem image) or on
// an opened file descriptor |fd|. The actual file-system size is |block_count|
// * |block_size| bytes. Returns true on success, false otherwise.
bool GetFilesystemSize(const std::string& device,
                       int* out_block_count,
                       int* out_block_size);
bool GetFilesystemSizeFromFD(int fd,
                             int* out_block_count,
                             int* out_block_size);

// Returns the path of the passed |command| on the board. This uses the
// environment variable SYSROOT to determine the path to the command on the
// board instead of the path on the running environment.
std::string GetPathOnBoard(const std::string& command);

// Returns a human-readable string with the file format based on magic constants
// on the header of the file.
std::string GetFileFormat(const std::string& path);

// Returns the string representation of the given UTC time.
// such as "11/14/2011 14:05:30 GMT".
std::string ToString(const base::Time utc_time);

// Returns true or false depending on the value of b.
std::string ToString(bool b);

// Returns a string representation of the given enum.
std::string ToString(DownloadSource source);

// Returns a string representation of the given enum.
std::string ToString(PayloadType payload_type);

enum BootLoader {
  BootLoader_SYSLINUX = 0,
  BootLoader_CHROME_FIRMWARE = 1
};
// Detects which bootloader this system uses and returns it via the out
// param. Returns true on success.
bool GetBootloader(BootLoader* out_bootloader);

// Schedules a Main Loop callback to trigger the crash reporter to perform an
// upload as if this process had crashed.
void ScheduleCrashReporterUpload();

// Fuzzes an integer |value| randomly in the range:
// [value - range / 2, value + range - range / 2]
int FuzzInt(int value, unsigned int range);

// Log a string in hex to LOG(INFO). Useful for debugging.
void HexDumpArray(const unsigned char* const arr, const size_t length);
inline void HexDumpString(const std::string& str) {
  HexDumpArray(reinterpret_cast<const unsigned char*>(str.data()), str.size());
}
inline void HexDumpVector(const std::vector<char>& vect) {
  HexDumpArray(reinterpret_cast<const unsigned char*>(&vect[0]), vect.size());
}

bool StringHasSuffix(const std::string& str, const std::string& suffix);
bool StringHasPrefix(const std::string& str, const std::string& prefix);

template<typename KeyType, typename ValueType>
bool MapContainsKey(const std::map<KeyType, ValueType>& m, const KeyType& k) {
  return m.find(k) != m.end();
}
template<typename KeyType>
bool SetContainsKey(const std::set<KeyType>& s, const KeyType& k) {
  return s.find(k) != s.end();
}

template<typename ValueType>
std::set<ValueType> SetWithValue(const ValueType& value) {
  std::set<ValueType> ret;
  ret.insert(value);
  return ret;
}

template<typename T>
bool VectorContainsValue(const std::vector<T>& vect, const T& value) {
  return std::find(vect.begin(), vect.end(), value) != vect.end();
}

template<typename T>
bool VectorIndexOf(const std::vector<T>& vect, const T& value,
                   typename std::vector<T>::size_type* out_index) {
  typename std::vector<T>::const_iterator it = std::find(vect.begin(),
                                                         vect.end(),
                                                         value);
  if (it == vect.end()) {
    return false;
  } else {
    *out_index = it - vect.begin();
    return true;
  }
}

template<typename ValueType>
void ApplyMap(std::vector<ValueType>* collection,
              const std::map<ValueType, ValueType>& the_map) {
  for (typename std::vector<ValueType>::iterator it = collection->begin();
       it != collection->end(); ++it) {
    typename std::map<ValueType, ValueType>::const_iterator map_it =
      the_map.find(*it);
    if (map_it != the_map.end()) {
      *it = map_it->second;
    }
  }
}

// Cgroups cpu shares constants. 1024 is the default shares a standard process
// gets and 2 is the minimum value. We set High as a value that gives the
// update-engine 2x the cpu share of a standard process.
enum CpuShares {
  kCpuSharesHigh = 2048,
  kCpuSharesNormal = 1024,
  kCpuSharesLow = 2,
};

// Compares cpu shares and returns an integer that is less
// than, equal to or greater than 0 if |shares_lhs| is,
// respectively, lower than, same as or higher than |shares_rhs|.
int CompareCpuShares(CpuShares shares_lhs,
                     CpuShares shares_rhs);

// Sets the current process shares to |shares|. Returns true on
// success, false otherwise.
bool SetCpuShares(CpuShares shares);

// Assumes |data| points to a Closure. Runs it and returns FALSE;
gboolean GlibRunClosure(gpointer data);

// Destroys the Closure pointed by |data|.
void GlibDestroyClosure(gpointer data);

// Converts seconds into human readable notation including days, hours, minutes
// and seconds. For example, 185 will yield 3m5s, 4300 will yield 1h11m40s, and
// 360000 will yield 4d4h0m0s.  Zero padding not applied. Seconds are always
// shown in the result.
std::string FormatSecs(unsigned secs);

// Converts a TimeDelta into human readable notation including days, hours,
// minutes, seconds and fractions of a second down to microsecond granularity,
// as necessary; for example, an output of 5d2h0m15.053s means that the input
// time was precise to the milliseconds only. Zero padding not applied, except
// for fractions. Seconds are always shown, but fractions thereof are only shown
// when applicable. If |delta| is negative, the output will have a leading '-'
// followed by the absolute duration.
std::string FormatTimeDelta(base::TimeDelta delta);

// This method transforms the given error code to be suitable for UMA and
// for error classification purposes by removing the higher order bits and
// aggregating error codes beyond the enum range, etc. This method is
// idempotent, i.e. if called with a value previously returned by this method,
// it'll return the same value again.
ErrorCode GetBaseErrorCode(ErrorCode code);

// Transforms a ErrorCode value into a metrics::DownloadErrorCode.
// This obviously only works for errors related to downloading so if |code|
// is e.g. |ErrorCode::kFilesystemCopierError| then
// |kDownloadErrorCodeInputMalformed| is returned.
metrics::DownloadErrorCode GetDownloadErrorCode(ErrorCode code);

// Transforms a ErrorCode value into a metrics::AttemptResult.
//
// If metrics::AttemptResult::kPayloadDownloadError is returned, you
// can use utils::GetDownloadError() to get more detail.
metrics::AttemptResult GetAttemptResult(ErrorCode code);

// Calculates the internet connection type given |type| and |tethering|.
metrics::ConnectionType GetConnectionType(NetworkConnectionType type,
                                          NetworkTethering tethering);

// Sends the error code to UMA using the metrics interface object in the given
// system state. It also uses the system state to determine the right UMA
// bucket for the error code.
void SendErrorCodeToUma(SystemState* system_state, ErrorCode code);

// Returns a string representation of the ErrorCodes (either the base
// error codes or the bit flags) for logging purposes.
std::string CodeToString(ErrorCode code);

// Creates the powerwash marker file with the appropriate commands in it.  Uses
// |file_path| as the path to the marker file if non-null, otherwise uses the
// global default. Returns true if successfully created.  False otherwise.
bool CreatePowerwashMarkerFile(const char* file_path);

// Deletes the marker file used to trigger Powerwash using clobber-state.  Uses
// |file_path| as the path to the marker file if non-null, otherwise uses the
// global default. Returns true if successfully deleted. False otherwise.
bool DeletePowerwashMarkerFile(const char* file_path);

// Assumes you want to install on the "other" device, where the other
// device is what you get if you swap 1 for 2 or 3 for 4 or vice versa
// for the number at the end of the boot device. E.g., /dev/sda1 -> /dev/sda2
// or /dev/sda4 -> /dev/sda3. See
// http://www.chromium.org/chromium-os/chromiumos-design-docs/disk-format
bool GetInstallDev(const std::string& boot_dev, std::string* install_dev);

// Checks if xattr is supported in the directory specified by
// |dir_path| which must be writable. Returns true if the feature is
// supported, false if not or if an error occured.
bool IsXAttrSupported(const base::FilePath& dir_path);

// Decodes the data in |base64_encoded| and stores it in a temporary
// file. Returns false if the given data is empty, not well-formed
// base64 or if an error occurred. If true is returned, the decoded
// data is stored in the file returned in |out_path|. The file should
// be deleted when no longer needed.
bool DecodeAndStoreBase64String(const std::string& base64_encoded,
                                base::FilePath *out_path);

// Converts |time| to an Omaha InstallDate which is defined as "the
// number of PST8PDT calendar weeks since Jan 1st 2007 0:00 PST, times
// seven" with PST8PDT defined as "Pacific Time" (e.g. UTC-07:00 if
// daylight savings is observed and UTC-08:00 otherwise.)
//
// If the passed in |time| variable is before Monday January 1st 2007
// 0:00 PST, False is returned and the value returned in
// |out_num_days| is undefined. Otherwise the number of PST8PDT
// calendar weeks since that date times seven is returned in
// |out_num_days| and the function returns True.
//
// (NOTE: This function does not currently take daylight savings time
// into account so the result may up to one hour off. This is because
// the glibc date and timezone routines depend on the TZ environment
// variable and changing environment variables is not thread-safe. An
// alternative, thread-safe API to use would be GDateTime/GTimeZone
// available in GLib 2.26 or later.)
bool ConvertToOmahaInstallDate(base::Time time, int *out_num_days);

// This function returns the duration on the wallclock since the last
// time it was called for the same |state_variable_key| value.
//
// If the function returns |true|, the duration (always non-negative)
// is returned in |out_duration|. If the function returns |false|
// something went wrong or there was no previous measurement.
bool WallclockDurationHelper(SystemState* system_state,
                             const std::string& state_variable_key,
                             base::TimeDelta* out_duration);

// This function returns the duration on the monotonic clock since the
// last time it was called for the same |storage| pointer.
//
// You should pass a pointer to a 64-bit integer in |storage| which
// should be initialized to 0.
//
// If the function returns |true|, the duration (always non-negative)
// is returned in |out_duration|. If the function returns |false|
// something went wrong or there was no previous measurement.
bool MonotonicDurationHelper(SystemState* system_state,
                             int64_t* storage,
                             base::TimeDelta* out_duration);

}  // namespace utils


// Class to unmount FS when object goes out of scope
class ScopedFilesystemUnmounter {
 public:
  explicit ScopedFilesystemUnmounter(const std::string& mountpoint)
      : mountpoint_(mountpoint),
        should_unmount_(true) {}
  ~ScopedFilesystemUnmounter() {
    if (should_unmount_) {
      utils::UnmountFilesystem(mountpoint_);
    }
  }
  void set_should_unmount(bool unmount) { should_unmount_ = unmount; }
 private:
  const std::string mountpoint_;
  bool should_unmount_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFilesystemUnmounter);
};

// Utility class to close a file descriptor
class ScopedFdCloser {
 public:
  explicit ScopedFdCloser(int* fd) : fd_(fd), should_close_(true) {}
  ~ScopedFdCloser() {
    if (should_close_ && fd_ && (*fd_ >= 0)) {
      if (!close(*fd_))
        *fd_ = -1;
    }
  }
  void set_should_close(bool should_close) { should_close_ = should_close; }
 private:
  int* fd_;
  bool should_close_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFdCloser);
};

// An EINTR-immune file descriptor closer.
class ScopedEintrSafeFdCloser {
 public:
  explicit ScopedEintrSafeFdCloser(int* fd) : fd_(fd), should_close_(true) {}
  ~ScopedEintrSafeFdCloser() {
    if (should_close_ && fd_ && (*fd_ >= 0)) {
      if (!IGNORE_EINTR(close(*fd_)))
        *fd_ = -1;
    }
  }
  void set_should_close(bool should_close) { should_close_ = should_close; }
 private:
  int* fd_;
  bool should_close_;
  DISALLOW_COPY_AND_ASSIGN(ScopedEintrSafeFdCloser);
};

// Utility class to delete a file when it goes out of scope.
class ScopedPathUnlinker {
 public:
  explicit ScopedPathUnlinker(const std::string& path)
      : path_(path),
        should_remove_(true) {}
  ~ScopedPathUnlinker() {
    if (should_remove_ && unlink(path_.c_str()) < 0) {
      std::string err_message = strerror(errno);
      LOG(ERROR) << "Unable to unlink path " << path_ << ": " << err_message;
    }
  }
  void set_should_remove(bool should_remove) { should_remove_ = should_remove; }

 private:
  const std::string path_;
  bool should_remove_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPathUnlinker);
};

// Utility class to delete an empty directory when it goes out of scope.
class ScopedDirRemover {
 public:
  explicit ScopedDirRemover(const std::string& path)
      : path_(path),
        should_remove_(true) {}
  ~ScopedDirRemover() {
    if (should_remove_ && (rmdir(path_.c_str()) < 0)) {
      PLOG(ERROR) << "Unable to remove dir " << path_;
    }
  }
  void set_should_remove(bool should_remove) { should_remove_ = should_remove; }

 protected:
  const std::string path_;

 private:
  bool should_remove_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDirRemover);
};

// Utility class to unmount a filesystem mounted on a temporary directory and
// delete the temporary directory when it goes out of scope.
class ScopedTempUnmounter : public ScopedDirRemover {
 public:
  explicit ScopedTempUnmounter(const std::string& path) :
      ScopedDirRemover(path) {}
  ~ScopedTempUnmounter() {
    utils::UnmountFilesystem(path_);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedTempUnmounter);
};

// A little object to call ActionComplete on the ActionProcessor when
// it's destructed.
class ScopedActionCompleter {
 public:
  explicit ScopedActionCompleter(ActionProcessor* processor,
                                 AbstractAction* action)
      : processor_(processor),
        action_(action),
        code_(ErrorCode::kError),
        should_complete_(true) {}
  ~ScopedActionCompleter() {
    if (should_complete_)
      processor_->ActionComplete(action_, code_);
  }
  void set_code(ErrorCode code) { code_ = code; }
  void set_should_complete(bool should_complete) {
    should_complete_ = should_complete;
  }
  ErrorCode get_code() const { return code_; }

 private:
  ActionProcessor* processor_;
  AbstractAction* action_;
  ErrorCode code_;
  bool should_complete_;
  DISALLOW_COPY_AND_ASSIGN(ScopedActionCompleter);
};

// A base::FreeDeleter that frees memory using g_free(). Useful when
// integrating with GLib since it can be used with std::unique_ptr to
// automatically free memory when going out of scope.
struct GLibFreeDeleter : public base::FreeDeleter {
  inline void operator()(void *ptr) const {
    g_free(reinterpret_cast<gpointer>(ptr));
  }
};

// A base::FreeDeleter that frees memory using g_strfreev(). Useful
// when integrating with GLib since it can be used with std::unique_ptr to
// automatically free memory when going out of scope.
struct GLibStrvFreeDeleter : public base::FreeDeleter {
  inline void operator()(void *ptr) const {
    g_strfreev(reinterpret_cast<gchar**>(ptr));
  }
};

}  // namespace chromeos_update_engine

#define TEST_AND_RETURN_FALSE_ERRNO(_x)                                        \
  do {                                                                         \
    bool _success = (_x);                                                      \
    if (!_success) {                                                           \
      std::string _msg =                                                       \
          chromeos_update_engine::utils::ErrnoNumberAsString(errno);           \
      LOG(ERROR) << #_x " failed: " << _msg;                                   \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_AND_RETURN_FALSE(_x)                                              \
  do {                                                                         \
    bool _success = (_x);                                                      \
    if (!_success) {                                                           \
      LOG(ERROR) << #_x " failed.";                                            \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_AND_RETURN_ERRNO(_x)                                              \
  do {                                                                         \
    bool _success = (_x);                                                      \
    if (!_success) {                                                           \
      std::string _msg =                                                       \
          chromeos_update_engine::utils::ErrnoNumberAsString(errno);           \
      LOG(ERROR) << #_x " failed: " << _msg;                                   \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define TEST_AND_RETURN(_x)                                                    \
  do {                                                                         \
    bool _success = (_x);                                                      \
    if (!_success) {                                                           \
      LOG(ERROR) << #_x " failed.";                                            \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define TEST_AND_RETURN_FALSE_ERRCODE(_x)                                      \
  do {                                                                         \
    errcode_t _error = (_x);                                                   \
    if (_error) {                                                              \
      errno = _error;                                                          \
      LOG(ERROR) << #_x " failed: " << _error;                                 \
      return false;                                                            \
    }                                                                          \
  } while (0)



#endif  // UPDATE_ENGINE_UTILS_H_
