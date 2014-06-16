// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <glib.h>
#include <gtest/gtest.h>

#include "update_engine/subprocess.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using base::TimeDelta;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class SubprocessTest : public ::testing::Test {
 protected:
  bool callback_done;
};

namespace {
int local_server_port = 0;

void Callback(int return_code, const string& output, void *p) {
  EXPECT_EQ(1, return_code);
  GMainLoop* loop = reinterpret_cast<GMainLoop*>(p);
  g_main_loop_quit(loop);
}

void CallbackEcho(int return_code, const string& output, void *p) {
  EXPECT_EQ(0, return_code);
  EXPECT_NE(string::npos, output.find("this is stdout"));
  EXPECT_NE(string::npos, output.find("this is stderr"));
  GMainLoop* loop = reinterpret_cast<GMainLoop*>(p);
  g_main_loop_quit(loop);
}

gboolean LaunchFalseInMainLoop(gpointer data) {
  vector<string> cmd;
  cmd.push_back("/bin/false");
  Subprocess::Get().Exec(cmd, Callback, data);
  return FALSE;
}

gboolean LaunchEchoInMainLoop(gpointer data) {
  vector<string> cmd;
  cmd.push_back("/bin/sh");
  cmd.push_back("-c");
  cmd.push_back("echo this is stdout; echo this is stderr > /dev/stderr");
  Subprocess::Get().Exec(cmd, CallbackEcho, data);
  return FALSE;
}
}  // namespace

TEST(SubprocessTest, SimpleTest) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
  g_timeout_add(0, &LaunchFalseInMainLoop, loop);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

TEST(SubprocessTest, EchoTest) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
  g_timeout_add(0, &LaunchEchoInMainLoop, loop);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

TEST(SubprocessTest, SynchronousEchoTest) {
  vector<string> cmd;
  cmd.push_back("/bin/sh");
  cmd.push_back("-c");
  cmd.push_back("echo -n stdout-here; echo -n stderr-there > /dev/stderr");
  int rc = -1;
  string stdout;
  ASSERT_TRUE(Subprocess::SynchronousExec(cmd, &rc, &stdout));
  EXPECT_EQ(0, rc);
  EXPECT_EQ("stdout-herestderr-there", stdout);
}

TEST(SubprocessTest, SynchronousEchoNoOutputTest) {
  vector<string> cmd;
  cmd.push_back("/bin/sh");
  cmd.push_back("-c");
  cmd.push_back("echo test");
  int rc = -1;
  ASSERT_TRUE(Subprocess::SynchronousExec(cmd, &rc, NULL));
  EXPECT_EQ(0, rc);
}

namespace {
void CallbackBad(int return_code, const string& output, void *p) {
  CHECK(false) << "should never be called.";
}

struct CancelTestData {
  bool spawned;
  GMainLoop *loop;
};

// TODO(garnold) this test method uses test_http_server as a representative for
// interactive processes that can be spawned/terminated at will. This causes us
// to go through hoops when spawning this process (e.g. obtaining the port
// number it uses so we can control it with wget). It would have been much
// preferred to use something else and thus simplify both test_http_server
// (doesn't have to be able to communicate through a temp file) and the test
// code below; for example, it sounds like a brain dead sleep loop with proper
// signal handlers could be used instead.
gboolean StartAndCancelInRunLoop(gpointer data) {
  CancelTestData* cancel_test_data = reinterpret_cast<CancelTestData*>(data);

  // Create a temp file for test_http_server to communicate its port number.
  char temp_file_name[] = "/tmp/subprocess_unittest-test_http_server-XXXXXX";
  int temp_fd = mkstemp(temp_file_name);
  CHECK_GE(temp_fd, 0);
  int temp_flags = fcntl(temp_fd, F_GETFL, 0) | O_NONBLOCK;
  CHECK_EQ(fcntl(temp_fd, F_SETFL, temp_flags), 0);

  vector<string> cmd;
  cmd.push_back("./test_http_server");
  cmd.push_back(temp_file_name);
  uint32_t tag = Subprocess::Get().Exec(cmd, CallbackBad, NULL);
  EXPECT_NE(0, tag);
  cancel_test_data->spawned = true;
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
    g_usleep(kSleepTime.InMicroseconds());
    total_wait_time += kSleepTime;
  }
  close(temp_fd);
  remove(temp_file_name);
  CHECK_GT(local_server_port, 0);
  LOG(INFO) << "server listening on port " << local_server_port;
  Subprocess::Get().CancelExec(tag);
  return FALSE;
}
}  // namespace

gboolean ExitWhenDone(gpointer data) {
  CancelTestData* cancel_test_data = reinterpret_cast<CancelTestData*>(data);
  if (cancel_test_data->spawned && !Subprocess::Get().SubprocessInFlight()) {
    // tear down the sub process
    printf("tear down time\n");
    int status = System(
        base::StringPrintf("wget -O /dev/null http://127.0.0.1:%d/quitquitquit",
                           local_server_port));
    EXPECT_NE(-1, status) << "system() failed";
    EXPECT_TRUE(WIFEXITED(status))
        << "command failed to run or died abnormally";
    g_main_loop_quit(cancel_test_data->loop);
    return FALSE;
  }
  return TRUE;
}

TEST(SubprocessTest, CancelTest) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
  CancelTestData cancel_test_data;
  cancel_test_data.spawned = false;
  cancel_test_data.loop = loop;
  g_timeout_add(100, &StartAndCancelInRunLoop, &cancel_test_data);
  g_timeout_add(10, &ExitWhenDone, &cancel_test_data);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

}  // namespace chromeos_update_engine
