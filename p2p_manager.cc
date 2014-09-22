// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides access to timestamps with nanosecond resolution in
// struct stat, See NOTES in stat(2) for details.
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "update_engine/p2p_manager.h"

#include <attr/xattr.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <linux/falloc.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "update_engine/glib_utils.h"
#include "update_engine/utils.h"

using base::FilePath;
using base::StringPrintf;
using base::Time;
using base::TimeDelta;
using std::map;
using std::pair;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

// The default p2p directory.
const char kDefaultP2PDir[] = "/var/cache/p2p";

// The p2p xattr used for conveying the final size of a file - see the
// p2p ddoc for details.
const char kCrosP2PFileSizeXAttrName[] = "user.cros-p2p-filesize";

}  // namespace

// The default P2PManager::Configuration implementation.
class ConfigurationImpl : public P2PManager::Configuration {
 public:
  ConfigurationImpl() {}

  virtual ~ConfigurationImpl() {}

  virtual base::FilePath GetP2PDir() {
    return base::FilePath(kDefaultP2PDir);
  }

  virtual vector<string> GetInitctlArgs(bool is_start) {
    vector<string> args;
    args.push_back("initctl");
    args.push_back(is_start ? "start" : "stop");
    args.push_back("p2p");
    return args;
  }

  virtual vector<string> GetP2PClientArgs(const string &file_id,
                                          size_t minimum_size) {
    vector<string> args;
    args.push_back("p2p-client");
    args.push_back(string("--get-url=") + file_id);
    args.push_back(base::StringPrintf("--minimum-size=%zu", minimum_size));
    return args;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationImpl);
};

// The default P2PManager implementation.
class P2PManagerImpl : public P2PManager {
 public:
  P2PManagerImpl(Configuration *configuration,
                 PrefsInterface *prefs,
                 const string& file_extension,
                 const int num_files_to_keep);

  // P2PManager methods.
  virtual void SetDevicePolicy(const policy::DevicePolicy* device_policy);
  virtual bool IsP2PEnabled();
  virtual bool EnsureP2PRunning();
  virtual bool EnsureP2PNotRunning();
  virtual bool PerformHousekeeping();
  virtual void LookupUrlForFile(const string& file_id,
                                size_t minimum_size,
                                TimeDelta max_time_to_wait,
                                LookupCallback callback);
  virtual bool FileShare(const string& file_id,
                         size_t expected_size);
  virtual base::FilePath FileGetPath(const string& file_id);
  virtual ssize_t FileGetSize(const string& file_id);
  virtual ssize_t FileGetExpectedSize(const string& file_id);
  virtual bool FileGetVisible(const string& file_id,
                              bool *out_result);
  virtual bool FileMakeVisible(const string& file_id);
  virtual int CountSharedFiles();

 private:
  // Enumeration for specifying visibility.
  enum Visibility {
    kVisible,
    kNonVisible
  };

  // Returns "." + |file_extension_| + ".p2p" if |visibility| is
  // |kVisible|. Returns the same concatenated with ".tmp" otherwise.
  string GetExt(Visibility visibility);

  // Gets the on-disk path for |file_id| depending on if the file
  // is visible or not.
  base::FilePath GetPath(const string& file_id, Visibility visibility);

  // Utility function used by EnsureP2PRunning() and EnsureP2PNotRunning().
  bool EnsureP2P(bool should_be_running);

  // The device policy being used or null if no policy is being used.
  const policy::DevicePolicy* device_policy_;

  // Configuration object.
  scoped_ptr<Configuration> configuration_;

  // Object for persisted state.
  PrefsInterface* prefs_;

  // A short string unique to the application (for example "cros_au")
  // used to mark a file as being owned by a particular application.
  const string file_extension_;

  // If non-zero, this number denotes how many files in /var/cache/p2p
  // owned by the application (cf. |file_extension_|) to keep after
  // performing housekeeping.
  const int num_files_to_keep_;

  // The string ".p2p".
  static const char kP2PExtension[];

  // The string ".tmp".
  static const char kTmpExtension[];

  DISALLOW_COPY_AND_ASSIGN(P2PManagerImpl);
};

const char P2PManagerImpl::kP2PExtension[] = ".p2p";

const char P2PManagerImpl::kTmpExtension[] = ".tmp";

P2PManagerImpl::P2PManagerImpl(Configuration *configuration,
                               PrefsInterface *prefs,
                               const string& file_extension,
                               const int num_files_to_keep)
  : device_policy_(nullptr),
    prefs_(prefs),
    file_extension_(file_extension),
    num_files_to_keep_(num_files_to_keep) {
  configuration_.reset(configuration != nullptr ? configuration :
                       new ConfigurationImpl());
}

void P2PManagerImpl::SetDevicePolicy(
    const policy::DevicePolicy* device_policy) {
  device_policy_ = device_policy;
}

bool P2PManagerImpl::IsP2PEnabled() {
  bool p2p_enabled = false;

  // The logic we want here is additive, e.g. p2p can be enabled by
  // either the crosh flag OR by Enterprise Policy, e.g. the following
  // truth table:
  //
  //  crosh_flag == FALSE  &&  enterprise_policy == unset  -> use_p2p == *
  //  crosh_flag == TRUE   &&  enterprise_policy == unset  -> use_p2p == TRUE
  //  crosh_flag == FALSE  &&  enterprise_policy == FALSE  -> use_p2p == FALSE
  //  crosh_flag == FALSE  &&  enterprise_policy == TRUE   -> use_p2p == TRUE
  //  crosh_flag == TRUE   &&  enterprise_policy == FALSE  -> use_p2p == TRUE
  //  crosh_flag == TRUE   &&  enterprise_policy == TRUE   -> use_p2p == TRUE
  //
  // *: TRUE if Enterprise Enrolled, FALSE otherwise.

  if (prefs_ != nullptr &&
      prefs_->Exists(kPrefsP2PEnabled) &&
      prefs_->GetBoolean(kPrefsP2PEnabled, &p2p_enabled) &&
      p2p_enabled) {
    LOG(INFO) << "The crosh flag indicates that p2p is enabled.";
    return true;
  }

  if (device_policy_ != nullptr) {
    if (device_policy_->GetAuP2PEnabled(&p2p_enabled)) {
      if (p2p_enabled) {
        LOG(INFO) << "Enterprise Policy indicates that p2p is enabled.";
        return true;
      }
    } else {
      // Enterprise-enrolled devices have an empty owner in their device policy.
      string owner;
      if (!device_policy_->GetOwner(&owner) || owner.empty()) {
        LOG(INFO) << "No p2p_enabled setting in Enterprise Policy but device "
                  << "is Enterprise Enrolled so allowing p2p.";
        return true;
      }
    }
  }

  LOG(INFO) << "Neither Enterprise Policy nor crosh flag indicates that p2p "
            << "is enabled.";
  return false;
}

bool P2PManagerImpl::EnsureP2P(bool should_be_running) {
  gchar *standard_error = nullptr;
  GError *error = nullptr;
  gint exit_status = 0;

  vector<string> args = configuration_->GetInitctlArgs(should_be_running);
  scoped_ptr<gchar*, GLibStrvFreeDeleter> argv(
      utils::StringVectorToGStrv(args));
  if (!g_spawn_sync(nullptr,  // working_directory
                    argv.get(),
                    nullptr,  // envp
                    static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH),
                    nullptr, nullptr,  // child_setup, user_data
                    nullptr,  // standard_output
                    &standard_error,
                    &exit_status,
                    &error)) {
    LOG(ERROR) << "Error spawning " << utils::StringVectorToString(args)
               << ": " << utils::GetAndFreeGError(&error);
    return false;
  }
  scoped_ptr<gchar, GLibFreeDeleter> standard_error_deleter(standard_error);

  if (!WIFEXITED(exit_status)) {
    LOG(ERROR) << "Error spawning '" << utils::StringVectorToString(args)
               << "': WIFEXITED is false";
    return false;
  }

  // If initctl(8) exits normally with exit status 0 ("success"), it
  // meant that it did what we requested.
  if (WEXITSTATUS(exit_status) == 0) {
    return true;
  }

  // Otherwise, screenscape stderr from initctl(8). Ugh, yes, this is
  // ugly but since the program lacks verbs/actions such as
  //
  //  ensure-started (or start-or-return-success-if-already-started)
  //  ensure-stopped (or stop-or-return-success-if-not-running)
  //
  // this is what we have to do.
  //
  // TODO(zeuthen,chromium:277051): Avoid doing this.
  const gchar *expected_error_message = should_be_running ?
    "initctl: Job is already running: p2p\n" :
    "initctl: Unknown instance \n";
  if (g_strcmp0(standard_error, expected_error_message) == 0) {
    return true;
  }

  return false;
}

bool P2PManagerImpl::EnsureP2PRunning() {
  return EnsureP2P(true);
}

bool P2PManagerImpl::EnsureP2PNotRunning() {
  return EnsureP2P(false);
}

// Returns True if the timestamp in the first pair is greater than the
// timestamp in the latter. If used with std::sort() this will yield a
// sequence of elements where newer (high timestamps) elements precede
// older ones (low timestamps).
static bool MatchCompareFunc(const pair<FilePath, Time>& a,
                             const pair<FilePath, Time>& b) {
  return a.second > b.second;
}

string P2PManagerImpl::GetExt(Visibility visibility) {
  string ext = string(".") + file_extension_ + kP2PExtension;
  switch (visibility) {
  case kVisible:
    break;
  case kNonVisible:
    ext += kTmpExtension;
    break;
  // Don't add a default case to let the compiler warn about newly
  // added enum values.
  }
  return ext;
}

FilePath P2PManagerImpl::GetPath(const string& file_id, Visibility visibility) {
  return configuration_->GetP2PDir().Append(file_id + GetExt(visibility));
}

bool P2PManagerImpl::PerformHousekeeping() {
  GDir* dir = nullptr;
  GError* error = nullptr;
  const char* name = nullptr;
  vector<pair<FilePath, Time>> matches;

  // Go through all files in the p2p dir and pick the ones that match
  // and get their ctime.
  base::FilePath p2p_dir = configuration_->GetP2PDir();
  dir = g_dir_open(p2p_dir.value().c_str(), 0, &error);
  if (dir == nullptr) {
    LOG(ERROR) << "Error opening directory " << p2p_dir.value() << ": "
               << utils::GetAndFreeGError(&error);
    return false;
  }

  if (num_files_to_keep_ == 0)
    return true;

  string ext_visible = GetExt(kVisible);
  string ext_non_visible = GetExt(kNonVisible);
  while ((name = g_dir_read_name(dir)) != nullptr) {
    if (!(g_str_has_suffix(name, ext_visible.c_str()) ||
          g_str_has_suffix(name, ext_non_visible.c_str())))
      continue;

    struct stat statbuf;
    base::FilePath file = p2p_dir.Append(name);
    if (stat(file.value().c_str(), &statbuf) != 0) {
      PLOG(ERROR) << "Error getting file status for " << file.value();
      continue;
    }

    Time time = utils::TimeFromStructTimespec(&statbuf.st_ctim);
    matches.push_back(std::make_pair(file, time));
  }
  g_dir_close(dir);

  // Sort list of matches, newest (biggest time) to oldest (lowest time).
  std::sort(matches.begin(), matches.end(), MatchCompareFunc);

  // Delete starting at element num_files_to_keep_.
  vector<pair<FilePath, Time>>::const_iterator i;
  for (i = matches.begin() + num_files_to_keep_; i < matches.end(); ++i) {
    const base::FilePath& file = i->first;
    LOG(INFO) << "Deleting p2p file " << file.value();
    if (unlink(file.value().c_str()) != 0) {
      PLOG(ERROR) << "Error deleting p2p file " << file.value();
      return false;
    }
  }

  return true;
}

// Helper class for implementing LookupUrlForFile().
class LookupData {
 public:
  explicit LookupData(P2PManager::LookupCallback callback)
    : callback_(callback),
      pid_(0),
      stdout_fd_(-1),
      stdout_channel_source_id_(0),
      child_watch_source_id_(0),
      timeout_source_id_(0),
      reported_(false) {}

  ~LookupData() {
    if (child_watch_source_id_ != 0)
      g_source_remove(child_watch_source_id_);
    if (stdout_channel_source_id_ != 0)
      g_source_remove(stdout_channel_source_id_);
    if (timeout_source_id_ != 0)
      g_source_remove(timeout_source_id_);
    if (stdout_fd_ != -1)
      close(stdout_fd_);
    if (pid_ != 0)
      kill(pid_, SIGTERM);
  }

  void InitiateLookup(gchar **argv, TimeDelta timeout) {
    // NOTE: if we fail early (i.e. in this method), we need to schedule
    // an idle to report the error. This is because we guarantee that
    // the callback is always called from the GLib mainloop (this
    // guarantee is useful for testing).

    GError *error = nullptr;
    if (!g_spawn_async_with_pipes(nullptr,  // working_directory
                                  argv,
                                  nullptr,  // envp
                                  static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH |
                                                     G_SPAWN_DO_NOT_REAP_CHILD),
                                  nullptr,  // child_setup
                                  this,
                                  &pid_,
                                  nullptr,  // standard_input
                                  &stdout_fd_,
                                  nullptr,  // standard_error
                                  &error)) {
      LOG(ERROR) << "Error spawning p2p-client: "
                 << utils::GetAndFreeGError(&error);
      ReportErrorAndDeleteInIdle();
      return;
    }

    GIOChannel* io_channel = g_io_channel_unix_new(stdout_fd_);
    stdout_channel_source_id_ = g_io_add_watch(
        io_channel,
        static_cast<GIOCondition>(G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP),
        OnIOChannelActivity, this);
    CHECK_NE(stdout_channel_source_id_, 0u);
    g_io_channel_unref(io_channel);

    child_watch_source_id_ = g_child_watch_add(pid_, OnChildWatchActivity,
                                               this);
    CHECK_NE(child_watch_source_id_, 0u);

    if (timeout.ToInternalValue() > 0) {
      timeout_source_id_ = g_timeout_add(timeout.InMilliseconds(),
                                         OnTimeout, this);
      CHECK_NE(timeout_source_id_, 0u);
    }
  }

 private:
  void ReportErrorAndDeleteInIdle() {
    g_idle_add(static_cast<GSourceFunc>(OnIdleForReportErrorAndDelete), this);
  }

  static gboolean OnIdleForReportErrorAndDelete(gpointer user_data) {
    LookupData *lookup_data = reinterpret_cast<LookupData*>(user_data);
    lookup_data->ReportError();
    delete lookup_data;
    return FALSE;  // Remove source.
  }

  void IssueCallback(const string& url) {
    if (!callback_.is_null())
      callback_.Run(url);
  }

  void ReportError() {
    if (reported_)
      return;
    IssueCallback("");
    reported_ = true;
  }

  void ReportSuccess() {
    if (reported_)
      return;

    string url = stdout_;
    size_t newline_pos = url.find('\n');
    if (newline_pos != string::npos)
      url.resize(newline_pos);

    // Since p2p-client(1) is constructing this URL itself strictly
    // speaking there's no need to validate it... but, anyway, can't
    // hurt.
    if (url.compare(0, 7, "http://") == 0) {
      IssueCallback(url);
    } else {
      LOG(ERROR) << "p2p URL '" << url << "' does not look right. Ignoring.";
      ReportError();
    }

    reported_ = true;
  }

  static gboolean OnIOChannelActivity(GIOChannel *source,
                                      GIOCondition condition,
                                      gpointer user_data) {
    LookupData *lookup_data = reinterpret_cast<LookupData*>(user_data);
    gchar* str = nullptr;
    GError* error = nullptr;
    GIOStatus status = g_io_channel_read_line(source,
                                              &str,
                                              nullptr,  // len
                                              nullptr,  // line_terminator
                                              &error);
    if (status != G_IO_STATUS_NORMAL) {
      // Ignore EOF since we usually get that before SIGCHLD and we
      // need to examine exit status there.
      if (status != G_IO_STATUS_EOF) {
        LOG(ERROR) << "Error reading a line from p2p-client: "
                   << utils::GetAndFreeGError(&error);
        lookup_data->ReportError();
        delete lookup_data;
      }
    } else {
      if (str != nullptr) {
        lookup_data->stdout_ += str;
        g_free(str);
      }
    }
    return TRUE;  // Don't remove source.
  }

  static void OnChildWatchActivity(GPid pid,
                                   gint status,
                                   gpointer user_data) {
    LookupData *lookup_data = reinterpret_cast<LookupData*>(user_data);

    if (!WIFEXITED(status)) {
      LOG(ERROR) << "Child didn't exit normally";
      lookup_data->ReportError();
    } else if (WEXITSTATUS(status) != 0) {
      LOG(INFO) << "Child exited with non-zero exit code "
                << WEXITSTATUS(status);
      lookup_data->ReportError();
    } else {
      lookup_data->ReportSuccess();
    }
    delete lookup_data;
  }

  static gboolean OnTimeout(gpointer user_data) {
    LookupData *lookup_data = reinterpret_cast<LookupData*>(user_data);
    lookup_data->ReportError();
    delete lookup_data;
    return TRUE;  // Don't remove source.
  }

  P2PManager::LookupCallback callback_;
  GPid pid_;
  gint stdout_fd_;
  guint stdout_channel_source_id_;
  guint child_watch_source_id_;
  guint timeout_source_id_;
  string stdout_;
  bool reported_;
};

void P2PManagerImpl::LookupUrlForFile(const string& file_id,
                                      size_t minimum_size,
                                      TimeDelta max_time_to_wait,
                                      LookupCallback callback) {
  LookupData *lookup_data = new LookupData(callback);
  string file_id_with_ext = file_id + "." + file_extension_;
  vector<string> args = configuration_->GetP2PClientArgs(file_id_with_ext,
                                                         minimum_size);
  gchar **argv = utils::StringVectorToGStrv(args);
  lookup_data->InitiateLookup(argv, max_time_to_wait);
  g_strfreev(argv);
}

bool P2PManagerImpl::FileShare(const string& file_id,
                               size_t expected_size) {
  // Check if file already exist.
  base::FilePath path = FileGetPath(file_id);
  if (!path.empty()) {
    // File exists - double check its expected size though.
    ssize_t file_expected_size = FileGetExpectedSize(file_id);
    if (file_expected_size == -1 ||
        static_cast<size_t>(file_expected_size) != expected_size) {
      LOG(ERROR) << "Existing p2p file " << path.value()
                 << " with expected_size=" << file_expected_size
                 << " does not match the passed in"
                 << " expected_size=" << expected_size;
      return false;
    }
    return true;
  }

  // Before creating the file, bail if statvfs(3) indicates that at
  // least twice the size is not available in P2P_DIR.
  struct statvfs statvfsbuf;
  base::FilePath p2p_dir = configuration_->GetP2PDir();
  if (statvfs(p2p_dir.value().c_str(), &statvfsbuf) != 0) {
    PLOG(ERROR) << "Error calling statvfs() for dir " << p2p_dir.value();
    return false;
  }
  size_t free_bytes =
      static_cast<size_t>(statvfsbuf.f_bsize) * statvfsbuf.f_bavail;
  if (free_bytes < 2 * expected_size) {
    // This can easily happen and is worth reporting.
    LOG(INFO) << "Refusing to allocate p2p file of " << expected_size
              << " bytes since the directory " << p2p_dir.value()
              << " only has " << free_bytes
              << " bytes available and this is less than twice the"
              << " requested size.";
    return false;
  }

  // Okie-dokey looks like enough space is available - create the file.
  path = GetPath(file_id, kNonVisible);
  int fd = open(path.value().c_str(), O_CREAT | O_RDWR, 0644);
  if (fd == -1) {
    PLOG(ERROR) << "Error creating file with path " << path.value();
    return false;
  }
  ScopedFdCloser fd_closer(&fd);

  // If the final size is known, allocate the file (e.g. reserve disk
  // space) and set the user.cros-p2p-filesize xattr.
  if (expected_size != 0) {
    if (fallocate(fd,
                  FALLOC_FL_KEEP_SIZE,  // Keep file size as 0.
                  0,
                  expected_size) != 0) {
      if (errno == ENOSYS || errno == EOPNOTSUPP) {
        // If the filesystem doesn't support the fallocate, keep
        // going. This is helpful when running unit tests on build
        // machines with ancient filesystems and/or OSes.
        PLOG(WARNING) << "Ignoring fallocate(2) failure";
      } else {
        // ENOSPC can happen (funky race though, cf. the statvfs() check
        // above), handle it gracefully, e.g. use logging level INFO.
        PLOG(INFO) << "Error allocating " << expected_size
                   << " bytes for file " << path.value();
        if (unlink(path.value().c_str()) != 0) {
          PLOG(ERROR) << "Error deleting file with path " << path.value();
        }
        return false;
      }
    }

    string decimal_size = base::StringPrintf("%zu", expected_size);
    if (fsetxattr(fd, kCrosP2PFileSizeXAttrName,
                  decimal_size.c_str(), decimal_size.size(), 0) != 0) {
      PLOG(ERROR) << "Error setting xattr " << path.value();
      return false;
    }
  }

  return true;
}

FilePath P2PManagerImpl::FileGetPath(const string& file_id) {
  struct stat statbuf;
  base::FilePath path;

  path = GetPath(file_id, kVisible);
  if (stat(path.value().c_str(), &statbuf) == 0) {
    return path;
  }

  path = GetPath(file_id, kNonVisible);
  if (stat(path.value().c_str(), &statbuf) == 0) {
    return path;
  }

  path.clear();
  return path;
}

bool P2PManagerImpl::FileGetVisible(const string& file_id,
                                    bool *out_result) {
  base::FilePath path = FileGetPath(file_id);
  if (path.empty()) {
    LOG(ERROR) << "No file for id " << file_id;
    return false;
  }
  if (out_result != nullptr)
    *out_result = path.MatchesExtension(kP2PExtension);
  return true;
}

bool P2PManagerImpl::FileMakeVisible(const string& file_id) {
  base::FilePath path = FileGetPath(file_id);
  if (path.empty()) {
    LOG(ERROR) << "No file for id " << file_id;
    return false;
  }

  // Already visible?
  if (path.MatchesExtension(kP2PExtension))
    return true;

  LOG_ASSERT(path.MatchesExtension(kTmpExtension));
  base::FilePath new_path = path.RemoveExtension();
  LOG_ASSERT(new_path.MatchesExtension(kP2PExtension));
  if (rename(path.value().c_str(), new_path.value().c_str()) != 0) {
    PLOG(ERROR) << "Error renaming " << path.value()
                << " to " << new_path.value();
    return false;
  }

  return true;
}

ssize_t P2PManagerImpl::FileGetSize(const string& file_id) {
  base::FilePath path = FileGetPath(file_id);
  if (path.empty())
    return -1;

  return utils::FileSize(path.value());
}

ssize_t P2PManagerImpl::FileGetExpectedSize(const string& file_id) {
  base::FilePath path = FileGetPath(file_id);
  if (path.empty())
    return -1;

  char ea_value[64] = { 0 };
  ssize_t ea_size;
  ea_size = getxattr(path.value().c_str(), kCrosP2PFileSizeXAttrName,
                     &ea_value, sizeof(ea_value) - 1);
  if (ea_size == -1) {
    PLOG(ERROR) << "Error calling getxattr() on file " << path.value();
    return -1;
  }

  char* endp = nullptr;
  long long int val = strtoll(ea_value, &endp, 0);  // NOLINT(runtime/int)
  if (*endp != '\0') {
    LOG(ERROR) << "Error parsing the value '" << ea_value
               << "' of the xattr " << kCrosP2PFileSizeXAttrName
               << " as an integer";
    return -1;
  }

  return val;
}

int P2PManagerImpl::CountSharedFiles() {
  GDir* dir;
  GError* error = nullptr;
  const char* name;
  int num_files = 0;

  base::FilePath p2p_dir = configuration_->GetP2PDir();
  dir = g_dir_open(p2p_dir.value().c_str(), 0, &error);
  if (dir == nullptr) {
    LOG(ERROR) << "Error opening directory " << p2p_dir.value() << ": "
               << utils::GetAndFreeGError(&error);
    return -1;
  }

  string ext_visible = GetExt(kVisible);
  string ext_non_visible = GetExt(kNonVisible);
  while ((name = g_dir_read_name(dir)) != nullptr) {
    if (g_str_has_suffix(name, ext_visible.c_str()) ||
        g_str_has_suffix(name, ext_non_visible.c_str())) {
      num_files += 1;
    }
  }
  g_dir_close(dir);

  return num_files;
}

P2PManager* P2PManager::Construct(Configuration *configuration,
                                  PrefsInterface *prefs,
                                  const string& file_extension,
                                  const int num_files_to_keep) {
  return new P2PManagerImpl(configuration,
                            prefs,
                            file_extension,
                            num_files_to_keep);
}

}  // namespace chromeos_update_engine
