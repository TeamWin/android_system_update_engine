// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/subprocess.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "update_engine/glib_utils.h"

using chromeos::MessageLoop;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

namespace chromeos_update_engine {

void Subprocess::GChildExitedCallback(GPid pid, gint status, gpointer data) {
  SubprocessRecord* record = reinterpret_cast<SubprocessRecord*>(data);

  // Make sure we read any remaining process output and then close the pipe.
  OnStdoutReady(record);

  MessageLoop::current()->CancelTask(record->task_id);
  record->task_id = MessageLoop::kTaskIdNull;
  if (IGNORE_EINTR(close(record->stdout_fd)) != 0) {
    PLOG(ERROR) << "Error closing fd " << record->stdout_fd;
  }
  g_spawn_close_pid(pid);
  gint use_status = status;
  if (WIFEXITED(status))
    use_status = WEXITSTATUS(status);

  if (status) {
    LOG(INFO) << "Subprocess status: " << use_status;
  }
  if (!record->stdout.empty()) {
    LOG(INFO) << "Subprocess output:\n" << record->stdout;
  }
  if (record->callback) {
    record->callback(use_status, record->stdout, record->callback_data);
  }
  Get().subprocess_records_.erase(record->tag);
}

void Subprocess::GRedirectStderrToStdout(gpointer user_data) {
  dup2(1, 2);
}

void Subprocess::OnStdoutReady(SubprocessRecord* record) {
  char buf[1024];
  ssize_t rc = 0;
  do {
    rc = HANDLE_EINTR(read(record->stdout_fd, buf, arraysize(buf)));
    if (rc < 0) {
      // EAGAIN and EWOULDBLOCK are normal return values when there's no more
      // input as we are in non-blocking mode.
      if (errno != EWOULDBLOCK && errno != EAGAIN) {
        PLOG(ERROR) << "Error reading fd " << record->stdout_fd;
      }
    } else {
      record->stdout.append(buf, rc);
    }
  } while (rc > 0);
}

namespace {
void FreeArgv(char** argv) {
  for (int i = 0; argv[i]; i++) {
    free(argv[i]);
    argv[i] = nullptr;
  }
}

void FreeArgvInError(char** argv) {
  FreeArgv(argv);
  LOG(ERROR) << "Ran out of memory copying args.";
}

// Note: Caller responsible for free()ing the returned value!
// Will return null on failure and free any allocated memory.
char** ArgPointer() {
  const char* keys[] = {"LD_LIBRARY_PATH", "PATH"};
  char** ret = new char*[arraysize(keys) + 1];
  int pointer = 0;
  for (size_t i = 0; i < arraysize(keys); i++) {
    if (getenv(keys[i])) {
      ret[pointer] = strdup(base::StringPrintf("%s=%s", keys[i],
                                               getenv(keys[i])).c_str());
      if (!ret[pointer]) {
        FreeArgv(ret);
        delete [] ret;
        return nullptr;
      }
      ++pointer;
    }
  }
  ret[pointer] = nullptr;
  return ret;
}

class ScopedFreeArgPointer {
 public:
  explicit ScopedFreeArgPointer(char** arr) : arr_(arr) {}
  ~ScopedFreeArgPointer() {
    if (!arr_)
      return;
    for (int i = 0; arr_[i]; i++)
      free(arr_[i]);
    delete[] arr_;
  }
 private:
  char** arr_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFreeArgPointer);
};
}  // namespace

uint32_t Subprocess::Exec(const vector<string>& cmd,
                          ExecCallback callback,
                          void* p) {
  return ExecFlags(cmd, static_cast<GSpawnFlags>(0), true, callback, p);
}

uint32_t Subprocess::ExecFlags(const vector<string>& cmd,
                               GSpawnFlags flags,
                               bool redirect_stderr_to_stdout,
                               ExecCallback callback,
                               void* p) {
  unique_ptr<gchar*, utils::GLibStrvFreeDeleter> argv(
       utils::StringVectorToGStrv(cmd));

  char** argp = ArgPointer();
  if (!argp) {
    FreeArgvInError(argv.get());  // null in argv[i] terminates argv.
    return 0;
  }
  ScopedFreeArgPointer argp_free(argp);

  shared_ptr<SubprocessRecord> record(new SubprocessRecord);
  record->callback = callback;
  record->callback_data = p;
  gint stdout_fd = -1;
  GError* error = nullptr;
  bool success = g_spawn_async_with_pipes(
      nullptr,  // working directory
      argv.get(),
      argp,
      static_cast<GSpawnFlags>(flags | G_SPAWN_DO_NOT_REAP_CHILD),  // flags
      // child setup function:
      redirect_stderr_to_stdout ? GRedirectStderrToStdout : nullptr,
      nullptr,  // child setup data pointer
      &record->pid,
      nullptr,
      &stdout_fd,
      nullptr,
      &error);
  if (!success) {
    LOG(ERROR) << "g_spawn_async failed: " << utils::GetAndFreeGError(&error);
    return 0;
  }
  record->tag =
      g_child_watch_add(record->pid, GChildExitedCallback, record.get());
  record->stdout_fd = stdout_fd;
  subprocess_records_[record->tag] = record;

  // Capture the subprocess output. Make our end of the pipe non-blocking.
  int fd_flags = fcntl(stdout_fd, F_GETFL, 0) | O_NONBLOCK;
  if (HANDLE_EINTR(fcntl(record->stdout_fd, F_SETFL, fd_flags)) < 0) {
    LOG(ERROR) << "Unable to set non-blocking I/O mode on fd "
               << record->stdout_fd << ".";
  }

  record->task_id = MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE,
      record->stdout_fd,
      MessageLoop::WatchMode::kWatchRead,
      true,
      base::Bind(&Subprocess::OnStdoutReady, record.get()));

  return record->tag;
}

void Subprocess::KillExec(uint32_t tag) {
  const auto& record = subprocess_records_.find(tag);
  if (record == subprocess_records_.end())
    return;
  record->second->callback = nullptr;
  kill(record->second->pid, SIGTERM);
}

bool Subprocess::SynchronousExecFlags(const vector<string>& cmd,
                                      GSpawnFlags flags,
                                      int* return_code,
                                      string* stdout) {
  if (stdout) {
    *stdout = "";
  }
  GError* err = nullptr;
  unique_ptr<char*[]> argv(new char*[cmd.size() + 1]);
  for (unsigned int i = 0; i < cmd.size(); i++) {
    argv[i] = strdup(cmd[i].c_str());
    if (!argv[i]) {
      FreeArgvInError(argv.get());  // null in argv[i] terminates argv.
      return false;
    }
  }
  argv[cmd.size()] = nullptr;

  char** argp = ArgPointer();
  if (!argp) {
    FreeArgvInError(argv.get());  // null in argv[i] terminates argv.
    return false;
  }
  ScopedFreeArgPointer argp_free(argp);

  char* child_stdout;
  bool success = g_spawn_sync(
      nullptr,  // working directory
      argv.get(),
      argp,
      static_cast<GSpawnFlags>(G_SPAWN_STDERR_TO_DEV_NULL |
                               G_SPAWN_SEARCH_PATH | flags),  // flags
      GRedirectStderrToStdout,  // child setup function
      nullptr,  // data for child setup function
      &child_stdout,
      nullptr,
      return_code,
      &err);
  FreeArgv(argv.get());
  LOG_IF(INFO, err) << utils::GetAndFreeGError(&err);
  if (child_stdout) {
    if (stdout) {
      *stdout = child_stdout;
    } else if (*child_stdout) {
      LOG(INFO) << "Subprocess output:\n" << child_stdout;
    }
    g_free(child_stdout);
  }
  return success;
}

bool Subprocess::SynchronousExec(const vector<string>& cmd,
                                 int* return_code,
                                 string* stdout) {
  return SynchronousExecFlags(cmd,
                              static_cast<GSpawnFlags>(0),
                              return_code,
                              stdout);
}

bool Subprocess::SubprocessInFlight() {
  for (const auto& tag_record_pair : subprocess_records_) {
    if (tag_record_pair.second->callback)
      return true;
  }
  return false;
}

Subprocess* Subprocess::subprocess_singleton_ = nullptr;

}  // namespace chromeos_update_engine
