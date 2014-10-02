// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_SUBPROCESS_H_
#define UPDATE_ENGINE_SUBPROCESS_H_

#include <unistd.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/logging.h>
#include <base/macros.h>
#include <chromeos/asynchronous_signal_handler_interface.h>
#include <chromeos/message_loops/message_loop.h>
#include <chromeos/process.h>
#include <chromeos/process_reaper.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

// The Subprocess class is a singleton. It's used to spawn off a subprocess
// and get notified when the subprocess exits. The result of Exec() can
// be saved and used to cancel the callback request and kill your process. If
// you know you won't call KillExec(), you may safely lose the return value
// from Exec().

// To create the Subprocess singleton just instantiate it with and call Init().
// You can't have two Subprocess instances initialized at the same time.

namespace chromeos_update_engine {

class Subprocess {
 public:
  enum Flags {
    kSearchPath = 1 << 0,
    kRedirectStderrToStdout = 1 << 1,
  };

  // Callback type used when an async process terminates. It receives the exit
  // code and the stdout output (and stderr if redirected).
  using ExecCallback = base::Callback<void(int, const std::string&)>;

  Subprocess() = default;

  // Destroy and unregister the Subprocess singleton.
  ~Subprocess();

  // Initialize and register the Subprocess singleton.
  void Init(chromeos::AsynchronousSignalHandlerInterface* async_signal_handler);

  // Launches a process in the background and calls the passed |callback| when
  // the process exits.
  // Returns the process id of the new launched process or 0 in case of failure.
  pid_t Exec(const std::vector<std::string>& cmd, const ExecCallback& callback);
  pid_t ExecFlags(const std::vector<std::string>& cmd,
                  uint32_t flags,
                  const ExecCallback& callback);

  // Kills the running process with SIGTERM and ignores the callback.
  void KillExec(pid_t tag);

  // Executes a command synchronously. Returns true on success. If |stdout| is
  // non-null, the process output is stored in it, otherwise the output is
  // logged. Note that stderr is redirected to stdout.
  static bool SynchronousExec(const std::vector<std::string>& cmd,
                              int* return_code,
                              std::string* stdout);
  static bool SynchronousExecFlags(const std::vector<std::string>& cmd,
                                   uint32_t flags,
                                   int* return_code,
                                   std::string* stdout);

  // Gets the one instance.
  static Subprocess& Get() {
    return *subprocess_singleton_;
  }

  // Returns true iff there is at least one subprocess we're waiting on.
  bool SubprocessInFlight();

 private:
  FRIEND_TEST(SubprocessTest, CancelTest);

  struct SubprocessRecord {
    explicit SubprocessRecord(const ExecCallback& callback)
      : callback(callback) {}

    // The callback supplied by the caller.
    ExecCallback callback;

    // The ProcessImpl instance managing the child process. Destroying this
    // will close our end of the pipes we have open.
    chromeos::ProcessImpl proc;

    // These are used to monitor the stdout of the running process, including
    // the stderr if it was redirected.
    chromeos::MessageLoop::TaskId stdout_task_id{
        chromeos::MessageLoop::kTaskIdNull};
    int stdout_fd{-1};
    std::string stdout;
  };

  // Callback which runs whenever there is input available on the subprocess
  // stdout pipe.
  static void OnStdoutReady(SubprocessRecord* record);

  // Callback for when any subprocess terminates. This calls the user
  // requested callback.
  void ChildExitedCallback(const siginfo_t& info);

  // The global instance.
  static Subprocess* subprocess_singleton_;

  // A map from the asynchronous subprocess tag (see Exec) to the subprocess
  // record structure for all active asynchronous subprocesses.
  std::map<pid_t, std::unique_ptr<SubprocessRecord>> subprocess_records_;

  // Used to watch for child processes.
  chromeos::ProcessReaper process_reaper_;

  DISALLOW_COPY_AND_ASSIGN(Subprocess);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_SUBPROCESS_H_
