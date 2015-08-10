// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/subprocess.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/location.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/message_loops/base_message_loop.h>
#include <chromeos/message_loops/message_loop.h>
#include <chromeos/message_loops/message_loop_utils.h>
#include <chromeos/strings/string_utils.h>
#include <gtest/gtest.h>

#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using base::TimeDelta;
using chromeos::MessageLoop;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class SubprocessTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_.SetAsCurrent();
    async_signal_handler_.Init();
    subprocess_.Init(&async_signal_handler_);
  }

  base::MessageLoopForIO base_loop_;
  chromeos::BaseMessageLoop loop_{&base_loop_};
  chromeos::AsynchronousSignalHandler async_signal_handler_;
  Subprocess subprocess_;
};

namespace {

int local_server_port = 0;

void ExpectedResults(int expected_return_code, const string& expected_output,
                     int return_code, const string& output) {
  EXPECT_EQ(expected_return_code, return_code);
  EXPECT_EQ(expected_output, output);
  MessageLoop::current()->BreakLoop();
}

void ExpectedEnvVars(int return_code, const string& output) {
  EXPECT_EQ(0, return_code);
  const std::set<string> allowed_envs = {"LD_LIBRARY_PATH", "PATH"};
  for (string key_value : chromeos::string_utils::Split(output, "\n")) {
    auto key_value_pair = chromeos::string_utils::SplitAtFirst(
        key_value, "=", true);
    EXPECT_NE(allowed_envs.end(), allowed_envs.find(key_value_pair.first));
  }
  MessageLoop::current()->BreakLoop();
}

}  // namespace

TEST_F(SubprocessTest, IsASingleton) {
  EXPECT_EQ(&subprocess_, &Subprocess::Get());
}

TEST_F(SubprocessTest, InactiveInstancesDontChangeTheSingleton) {
  std::unique_ptr<Subprocess> another_subprocess(new Subprocess());
  EXPECT_EQ(&subprocess_, &Subprocess::Get());
  another_subprocess.reset();
  EXPECT_EQ(&subprocess_, &Subprocess::Get());
}

TEST_F(SubprocessTest, SimpleTest) {
  EXPECT_TRUE(subprocess_.Exec({"/bin/false"},
                               base::Bind(&ExpectedResults, 1, "")));
  loop_.Run();
}

TEST_F(SubprocessTest, EchoTest) {
  EXPECT_TRUE(subprocess_.Exec(
      {"/bin/sh", "-c", "echo this is stdout; echo this is stderr >&2"},
      base::Bind(&ExpectedResults, 0, "this is stdout\nthis is stderr\n")));
  loop_.Run();
}

TEST_F(SubprocessTest, StderrNotIncludedInOutputTest) {
  EXPECT_TRUE(subprocess_.ExecFlags(
      {"/bin/sh", "-c", "echo on stdout; echo on stderr >&2"},
      0,
      base::Bind(&ExpectedResults, 0, "on stdout\n")));
  loop_.Run();
}

TEST_F(SubprocessTest, EnvVarsAreFiltered) {
  EXPECT_TRUE(subprocess_.Exec({"/usr/bin/env"}, base::Bind(&ExpectedEnvVars)));
  loop_.Run();
}

TEST_F(SubprocessTest, SynchronousTrueSearchsOnPath) {
  int rc = -1;
  EXPECT_TRUE(Subprocess::SynchronousExecFlags(
      {"true"}, Subprocess::kSearchPath, &rc, nullptr));
  EXPECT_EQ(0, rc);
}

TEST_F(SubprocessTest, SynchronousEchoTest) {
  vector<string> cmd = {
    "/bin/sh",
    "-c",
    "echo -n stdout-here; echo -n stderr-there > /dev/stderr"};
  int rc = -1;
  string stdout;
  ASSERT_TRUE(Subprocess::SynchronousExec(cmd, &rc, &stdout));
  EXPECT_EQ(0, rc);
  EXPECT_EQ("stdout-herestderr-there", stdout);
}

TEST_F(SubprocessTest, SynchronousEchoNoOutputTest) {
  int rc = -1;
  ASSERT_TRUE(Subprocess::SynchronousExec(
      {"/bin/sh", "-c", "echo test"},
      &rc, nullptr));
  EXPECT_EQ(0, rc);
}

namespace {
void CallbackBad(int return_code, const string& output) {
  ADD_FAILURE() << "should never be called.";
}

// TODO(garnold) this test method uses test_http_server as a representative for
// interactive processes that can be spawned/terminated at will. This causes us
// to go through hoops when spawning this process (e.g. obtaining the port
// number it uses so we can control it with wget). It would have been much
// preferred to use something else and thus simplify both test_http_server
// (doesn't have to be able to communicate through a temp file) and the test
// code below; for example, it sounds like a brain dead sleep loop with proper
// signal handlers could be used instead.
void StartAndCancelInRunLoop(bool* spawned) {
  // Create a temp file for test_http_server to communicate its port number.
  char temp_file_name[] = "/tmp/subprocess_unittest-test_http_server-XXXXXX";
  int temp_fd = mkstemp(temp_file_name);
  CHECK_GE(temp_fd, 0);
  int temp_flags = fcntl(temp_fd, F_GETFL, 0) | O_NONBLOCK;
  CHECK_EQ(fcntl(temp_fd, F_SETFL, temp_flags), 0);

  vector<string> cmd;
  cmd.push_back("./test_http_server");
  cmd.push_back(temp_file_name);
  uint32_t tag = Subprocess::Get().Exec(cmd, base::Bind(&CallbackBad));
  EXPECT_NE(0, tag);
  *spawned = true;
  printf("test http server spawned\n");
  // Wait for server to be up and running
  TimeDelta total_wait_time;
  const TimeDelta kSleepTime = TimeDelta::FromMilliseconds(100);
  const TimeDelta kMaxWaitTime = TimeDelta::FromSeconds(3);
  local_server_port = 0;
  static const char* kServerListeningMsgPrefix = "listening on port ";
  while (total_wait_time.InMicroseconds() < kMaxWaitTime.InMicroseconds()) {
    char line[80];
    int line_len = read(temp_fd, line, sizeof(line) - 1);
    if (line_len > 0) {
      line[line_len] = '\0';
      CHECK_EQ(strstr(line, kServerListeningMsgPrefix), line);
      const char* listening_port_str =
          line + strlen(kServerListeningMsgPrefix);
      char* end_ptr;
      long raw_port = strtol(listening_port_str,  // NOLINT(runtime/int)
                             &end_ptr, 10);
      CHECK(!*end_ptr || *end_ptr == '\n');
      local_server_port = static_cast<in_port_t>(raw_port);
      break;
    } else if (line_len < 0 && errno != EAGAIN) {
      LOG(INFO) << "error reading from " << temp_file_name << ": "
                << strerror(errno);
      break;
    }
    usleep(kSleepTime.InMicroseconds());
    total_wait_time += kSleepTime;
  }
  close(temp_fd);
  remove(temp_file_name);
  CHECK_GT(local_server_port, 0);
  LOG(INFO) << "server listening on port " << local_server_port;
  Subprocess::Get().KillExec(tag);
}

void ExitWhenDone(bool* spawned) {
  if (*spawned && !Subprocess::Get().SubprocessInFlight()) {
    // tear down the sub process
    printf("tear down time\n");
    int status = test_utils::System(
        base::StringPrintf("wget -O /dev/null http://127.0.0.1:%d/quitquitquit",
                           local_server_port));
    EXPECT_NE(-1, status) << "system() failed";
    EXPECT_TRUE(WIFEXITED(status))
        << "command failed to run or died abnormally";
    MessageLoop::current()->BreakLoop();
  } else {
    // Re-run this callback again in 10 ms.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ExitWhenDone, spawned),
        TimeDelta::FromMilliseconds(10));
  }
}

}  // namespace

TEST_F(SubprocessTest, CancelTest) {
  bool spawned = false;
  loop_.PostDelayedTask(
      FROM_HERE,
      base::Bind(&StartAndCancelInRunLoop, &spawned),
      TimeDelta::FromMilliseconds(100));
  loop_.PostDelayedTask(
      FROM_HERE,
      base::Bind(&ExitWhenDone, &spawned),
      TimeDelta::FromMilliseconds(10));
  loop_.Run();
  // This test would leak a callback that runs when the child process exits
  // unless we wait for it to run.
  chromeos::MessageLoopRunUntil(
      &loop_,
      TimeDelta::FromSeconds(10),
      base::Bind([] {
        return Subprocess::Get().subprocess_records_.empty();
      }));
}

}  // namespace chromeos_update_engine
