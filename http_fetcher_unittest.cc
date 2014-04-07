// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <glib.h>
#include <gtest/gtest.h>

#include "update_engine/http_common.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/mock_connection_manager.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/mock_system_state.h"
#include "update_engine/multi_range_http_fetcher.h"
#include "update_engine/proxy_resolver.h"
#include "update_engine/utils.h"

using std::make_pair;
using std::pair;
using std::string;
using std::vector;

using base::TimeDelta;
using testing::_;
using testing::SetArgumentPointee;
using testing::DoAll;
using testing::Return;

namespace {

const int kBigLength           = 100000;
const int kMediumLength        = 1000;
const int kFlakyTruncateLength = 29000;
const int kFlakySleepEvery     = 3;
const int kFlakySleepSecs      = 10;

}  // namespace

namespace chromeos_update_engine {

static const char *kUnusedUrl = "unused://unused";

static inline string LocalServerUrlForPath(in_port_t port,
                                           const string& path) {
  string port_str = (port ? base::StringPrintf(":%hu", port) : "");
  return base::StringPrintf("http://127.0.0.1%s%s", port_str.c_str(),
                            path.c_str());
}

//
// Class hierarchy for HTTP server implementations.
//

class HttpServer {
 public:
  // This makes it an abstract class (dirty but works).
  virtual ~HttpServer() = 0;

  virtual in_port_t GetPort() const {
    return 0;
  }

  bool started_;
};

HttpServer::~HttpServer() {}


class NullHttpServer : public HttpServer {
 public:
  NullHttpServer() {
    started_ = true;
  }
};


class PythonHttpServer : public HttpServer {
 public:
  PythonHttpServer() : pid_(-1), port_(0) {
    started_ = false;

    // Spawn the server process.
    gchar *argv[] = {
      const_cast<gchar*>("./test_http_server"),
      NULL };
    GError *err;
    gint server_stdout = -1;
    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                                  NULL, NULL, &pid_, NULL, &server_stdout, NULL,
                                  &err)) {
      LOG(ERROR) << "failed to spawn http server process";
      return;
    }
    CHECK_GT(pid_, 0);
    CHECK_GE(server_stdout, 0);
    LOG(INFO) << "started http server with pid " << pid_;

    // Wait for server to begin accepting connections, obtain its port.
    char line[80];
    const size_t listening_msg_prefix_len = strlen(kServerListeningMsgPrefix);
    CHECK_GT(sizeof(line), listening_msg_prefix_len);
    int line_len = read(server_stdout, line, sizeof(line) - 1);
    if (line_len <= static_cast<int>(listening_msg_prefix_len)) {
      if (line_len < 0) {
        LOG(ERROR) << "error reading http server stdout: "
                   << strerror(errno);
      } else {
        LOG(ERROR) << "server output too short";
      }
      Terminate(true);
      return;
    }

    line[line_len] = '\0';
    CHECK_EQ(strstr(line, kServerListeningMsgPrefix), line);
    const char* listening_port_str = line + listening_msg_prefix_len;
    char* end_ptr;
    long raw_port = strtol(listening_port_str, &end_ptr, 10);
    CHECK(!*end_ptr || *end_ptr == '\n');
    port_ = static_cast<in_port_t>(raw_port);
    CHECK_GT(port_, 0);
    started_ = true;
    LOG(INFO) << "server running, listening on port " << port_;
    LOG(INFO) << "gdb attach now!";
  }

  ~PythonHttpServer() {
    // If there's no process, do nothing.
    if (pid_ == -1)
      return;

    // If server is responsive, request that it gracefully terminate.
    bool do_kill = false;
    if (started_) {
      LOG(INFO) << "running wget to exit";
      if (system((string("wget -t 1 --output-document=/dev/null ") +
                  LocalServerUrlForPath(port_, "/quitquitquit")).c_str())) {
        LOG(WARNING) << "wget failed, resorting to brute force";
        do_kill = true;
      }
    }

    // Server not responding or wget failed, kill the process.
    Terminate(do_kill);
  }

  virtual in_port_t GetPort() const {
    return port_;
  }

 private:
  void Terminate(bool do_kill) {
    ASSERT_GT(pid_, 0);

    if (do_kill) {
      LOG(INFO) << "terminating (SIGKILL) server process with pid " << pid_;
      kill(pid_, SIGKILL);
    }

    LOG(INFO) << "waiting for http server with pid " << pid_ << " to terminate";
    int status;
    pid_t killed_pid = waitpid(pid_, &status, 0);
    ASSERT_EQ(killed_pid, pid_);
    LOG(INFO) << "http server with pid " << pid_
              << " terminated with status " << status;
    pid_ = -1;
  }

  static const char* kServerListeningMsgPrefix;

  GPid pid_;
  in_port_t port_;
};

const char* PythonHttpServer::kServerListeningMsgPrefix = "listening on port ";

//
// Class hierarchy for HTTP fetcher test wrappers.
//

class AnyHttpFetcherTest {
 public:
  AnyHttpFetcherTest()
      : mock_connection_manager_(&mock_system_state_) {
    mock_system_state_.set_connection_manager(&mock_connection_manager_);
  }

  virtual HttpFetcher* NewLargeFetcher(size_t num_proxies) = 0;
  HttpFetcher* NewLargeFetcher() {
    return NewLargeFetcher(1);
  }

  virtual HttpFetcher* NewSmallFetcher(size_t num_proxies) = 0;
  HttpFetcher* NewSmallFetcher() {
    return NewSmallFetcher(1);
  }

  virtual string BigUrl(in_port_t port) const { return kUnusedUrl; }
  virtual string SmallUrl(in_port_t port) const { return kUnusedUrl; }
  virtual string ErrorUrl(in_port_t port) const { return kUnusedUrl; }

  virtual bool IsMock() const = 0;
  virtual bool IsMulti() const = 0;

  virtual void IgnoreServerAborting(HttpServer* server) const {}

  virtual HttpServer *CreateServer() = 0;

 protected:
  DirectProxyResolver proxy_resolver_;
  MockSystemState mock_system_state_;
  MockConnectionManager mock_connection_manager_;
};

class MockHttpFetcherTest : public AnyHttpFetcherTest {
 public:
  // Necessary to unhide the definition in the base class.
  using AnyHttpFetcherTest::NewLargeFetcher;
  virtual HttpFetcher* NewLargeFetcher(size_t num_proxies) {
    vector<char> big_data(1000000);
    CHECK(num_proxies > 0);
    proxy_resolver_.set_num_proxies(num_proxies);
    return new MockHttpFetcher(
        big_data.data(),
        big_data.size(),
        reinterpret_cast<ProxyResolver*>(&proxy_resolver_));
  }

  // Necessary to unhide the definition in the base class.
  using AnyHttpFetcherTest::NewSmallFetcher;
  virtual HttpFetcher* NewSmallFetcher(size_t num_proxies) {
    CHECK(num_proxies > 0);
    proxy_resolver_.set_num_proxies(num_proxies);
    return new MockHttpFetcher(
        "x",
        1,
        reinterpret_cast<ProxyResolver*>(&proxy_resolver_));
  }

  virtual bool IsMock() const { return true; }
  virtual bool IsMulti() const { return false; }

  virtual HttpServer *CreateServer() {
    return new NullHttpServer;
  }
};

class LibcurlHttpFetcherTest : public AnyHttpFetcherTest {
 public:
  // Necessary to unhide the definition in the base class.
  using AnyHttpFetcherTest::NewLargeFetcher;
  virtual HttpFetcher* NewLargeFetcher(size_t num_proxies) {
    CHECK(num_proxies > 0);
    proxy_resolver_.set_num_proxies(num_proxies);
    LibcurlHttpFetcher *ret = new
        LibcurlHttpFetcher(reinterpret_cast<ProxyResolver*>(&proxy_resolver_),
                           &mock_system_state_, false);
    // Speed up test execution.
    ret->set_idle_seconds(1);
    ret->set_retry_seconds(1);
    mock_system_state_.fake_hardware()->SetIsOfficialBuild(false);
    return ret;
  }

  // Necessary to unhide the definition in the base class.
  using AnyHttpFetcherTest::NewSmallFetcher;
  virtual HttpFetcher* NewSmallFetcher(size_t num_proxies) {
    return NewLargeFetcher(num_proxies);
  }

  virtual string BigUrl(in_port_t port) const {
    return LocalServerUrlForPath(port,
                                 base::StringPrintf("/download/%d",
                                                    kBigLength));
  }
  virtual string SmallUrl(in_port_t port) const {
    return LocalServerUrlForPath(port, "/foo");
  }
  virtual string ErrorUrl(in_port_t port) const {
    return LocalServerUrlForPath(port, "/error");
  }

  virtual bool IsMock() const { return false; }
  virtual bool IsMulti() const { return false; }

  virtual void IgnoreServerAborting(HttpServer* server) const {
    // Nothing to do.
  }

  virtual HttpServer *CreateServer() {
    return new PythonHttpServer;
  }
};

class MultiRangeHttpFetcherTest : public LibcurlHttpFetcherTest {
 public:
  // Necessary to unhide the definition in the base class.
  using AnyHttpFetcherTest::NewLargeFetcher;
  virtual HttpFetcher* NewLargeFetcher(size_t num_proxies) {
    CHECK(num_proxies > 0);
    proxy_resolver_.set_num_proxies(num_proxies);
    ProxyResolver* resolver =
        reinterpret_cast<ProxyResolver*>(&proxy_resolver_);
    MultiRangeHttpFetcher *ret =
        new MultiRangeHttpFetcher(
            new LibcurlHttpFetcher(resolver, &mock_system_state_, false));
    ret->ClearRanges();
    ret->AddRange(0);
    // Speed up test execution.
    ret->set_idle_seconds(1);
    ret->set_retry_seconds(1);
    mock_system_state_.fake_hardware()->SetIsOfficialBuild(false);
    return ret;
  }

  // Necessary to unhide the definition in the base class.
  using AnyHttpFetcherTest::NewSmallFetcher;
  virtual HttpFetcher* NewSmallFetcher(size_t num_proxies) {
    return NewLargeFetcher(num_proxies);
  }

  virtual bool IsMulti() const { return true; }
};


//
// Infrastructure for type tests of HTTP fetcher.
// See: http://code.google.com/p/googletest/wiki/AdvancedGuide#Typed_Tests
//

// Fixture class template. We use an explicit constraint to guarantee that it
// can only be instantiated with an AnyHttpFetcherTest type, see:
// http://www2.research.att.com/~bs/bs_faq2.html#constraints
template <typename T>
class HttpFetcherTest : public ::testing::Test {
 public:
  T test_;

 private:
  static void TypeConstraint(T *a) {
    AnyHttpFetcherTest *b = a;
    if (b == 0) // Silence compiler warning of unused variable.
      *b = a;
  }
};

// Test case types list.
typedef ::testing::Types<LibcurlHttpFetcherTest,
                         MockHttpFetcherTest,
                         MultiRangeHttpFetcherTest> HttpFetcherTestTypes;
TYPED_TEST_CASE(HttpFetcherTest, HttpFetcherTestTypes);


namespace {
class HttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  HttpFetcherTestDelegate() :
      is_expect_error_(false), times_transfer_complete_called_(0),
      times_transfer_terminated_called_(0), times_received_bytes_called_(0) {}

  virtual void ReceivedBytes(HttpFetcher* /* fetcher */,
                             const char* /* bytes */, int /* length */) {
    // Update counters
    times_received_bytes_called_++;
  }

  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    if (is_expect_error_)
      EXPECT_EQ(kHttpResponseNotFound, fetcher->http_response_code());
    else
      EXPECT_EQ(kHttpResponseOk, fetcher->http_response_code());
    g_main_loop_quit(loop_);

    // Update counter
    times_transfer_complete_called_++;
  }

  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
    times_transfer_terminated_called_++;
  }

  GMainLoop* loop_;

  // Are we expecting an error response? (default: no)
  bool is_expect_error_;

  // Counters for callback invocations.
  int times_transfer_complete_called_;
  int times_transfer_terminated_called_;
  int times_received_bytes_called_;
};

struct StartTransferArgs {
  HttpFetcher *http_fetcher;
  string url;
};

gboolean StartTransfer(gpointer data) {
  StartTransferArgs *args = reinterpret_cast<StartTransferArgs*>(data);
  args->http_fetcher->BeginTransfer(args->url);
  return FALSE;
}
}  // namespace {}

TYPED_TEST(HttpFetcherTest, SimpleTest) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    HttpFetcherTestDelegate delegate;
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(this->test_.NewSmallFetcher());
    fetcher->set_delegate(&delegate);

    MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
        fetcher->GetSystemState()->connection_manager());
    EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetWifi),
                            SetArgumentPointee<2>(NetworkTethering::kUnknown),
                            Return(true)));
    EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetWifi, _))
      .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_cm, StringForConnectionType(kNetWifi))
      .WillRepeatedly(Return(shill::kTypeWifi));

    scoped_ptr<HttpServer> server(this->test_.CreateServer());
    ASSERT_TRUE(server->started_);

    StartTransferArgs start_xfer_args = {
      fetcher.get(), this->test_.SmallUrl(server->GetPort())};

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);
  }
  g_main_loop_unref(loop);
}

TYPED_TEST(HttpFetcherTest, SimpleBigTest) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    HttpFetcherTestDelegate delegate;
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(this->test_.NewLargeFetcher());
    fetcher->set_delegate(&delegate);

    MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
        fetcher->GetSystemState()->connection_manager());
    EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetEthernet),
                            SetArgumentPointee<2>(NetworkTethering::kUnknown),
                            Return(true)));
    EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetEthernet, _))
      .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_cm, StringForConnectionType(kNetEthernet))
      .WillRepeatedly(Return(shill::kTypeEthernet));

    scoped_ptr<HttpServer> server(this->test_.CreateServer());
    ASSERT_TRUE(server->started_);

    StartTransferArgs start_xfer_args = {
      fetcher.get(), this->test_.BigUrl(server->GetPort())};

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);
  }
  g_main_loop_unref(loop);
}

// Issue #9648: when server returns an error HTTP response, the fetcher needs to
// terminate transfer prematurely, rather than try to process the error payload.
TYPED_TEST(HttpFetcherTest, ErrorTest) {
  if (this->test_.IsMock() || this->test_.IsMulti())
    return;
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    HttpFetcherTestDelegate delegate;
    delegate.loop_ = loop;

    // Delegate should expect an error response.
    delegate.is_expect_error_ = true;

    scoped_ptr<HttpFetcher> fetcher(this->test_.NewSmallFetcher());
    fetcher->set_delegate(&delegate);

    MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
        fetcher->GetSystemState()->connection_manager());
    EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetWimax),
                            SetArgumentPointee<2>(NetworkTethering::kUnknown),
                            Return(true)));
    EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetWimax, _))
      .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_cm, StringForConnectionType(kNetWimax))
      .WillRepeatedly(Return(shill::kTypeWimax));

    scoped_ptr<HttpServer> server(this->test_.CreateServer());
    ASSERT_TRUE(server->started_);

    StartTransferArgs start_xfer_args = {
      fetcher.get(),
      this->test_.ErrorUrl(server->GetPort())
    };

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);

    // Make sure that no bytes were received.
    CHECK_EQ(delegate.times_received_bytes_called_, 0);
    CHECK_EQ(fetcher->GetBytesDownloaded(), static_cast<size_t>(0));

    // Make sure that transfer completion was signaled once, and no termination
    // was signaled.
    CHECK_EQ(delegate.times_transfer_complete_called_, 1);
    CHECK_EQ(delegate.times_transfer_terminated_called_, 0);
  }
  g_main_loop_unref(loop);
}

namespace {
class PausingHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* /* bytes */, int /* length */) {
    CHECK(!paused_);
    paused_ = true;
    fetcher->Pause();
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  void Unpause() {
    CHECK(paused_);
    paused_ = false;
    fetcher_->Unpause();
  }
  bool paused_;
  HttpFetcher* fetcher_;
  GMainLoop* loop_;
};

gboolean UnpausingTimeoutCallback(gpointer data) {
  PausingHttpFetcherTestDelegate *delegate =
      reinterpret_cast<PausingHttpFetcherTestDelegate*>(data);
  if (delegate->paused_)
    delegate->Unpause();
  return TRUE;
}
}  // namespace {}

TYPED_TEST(HttpFetcherTest, PauseTest) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    PausingHttpFetcherTestDelegate delegate;
    scoped_ptr<HttpFetcher> fetcher(this->test_.NewLargeFetcher());
    delegate.paused_ = false;
    delegate.loop_ = loop;
    delegate.fetcher_ = fetcher.get();
    fetcher->set_delegate(&delegate);

    MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
        fetcher->GetSystemState()->connection_manager());
    EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetCellular),
                            SetArgumentPointee<2>(NetworkTethering::kUnknown),
                            Return(true)));
    EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetCellular, _))
      .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_cm, StringForConnectionType(kNetCellular))
      .WillRepeatedly(Return(shill::kTypeCellular));

    scoped_ptr<HttpServer> server(this->test_.CreateServer());
    ASSERT_TRUE(server->started_);

    guint callback_id = g_timeout_add(kHttpResponseInternalServerError,
                                      UnpausingTimeoutCallback, &delegate);
    fetcher->BeginTransfer(this->test_.BigUrl(server->GetPort()));

    g_main_loop_run(loop);
    g_source_remove(callback_id);
  }
  g_main_loop_unref(loop);
}

namespace {
class AbortingHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {}
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    ADD_FAILURE();  // We should never get here
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    EXPECT_EQ(fetcher, fetcher_.get());
    EXPECT_FALSE(once_);
    EXPECT_TRUE(callback_once_);
    callback_once_ = false;
    // |fetcher| can be destroyed during this callback.
    fetcher_.reset(NULL);
  }
  void TerminateTransfer() {
    CHECK(once_);
    once_ = false;
    fetcher_->TerminateTransfer();
  }
  void EndLoop() {
    g_main_loop_quit(loop_);
  }
  bool once_;
  bool callback_once_;
  scoped_ptr<HttpFetcher> fetcher_;
  GMainLoop* loop_;
};

gboolean AbortingTimeoutCallback(gpointer data) {
  AbortingHttpFetcherTestDelegate *delegate =
      reinterpret_cast<AbortingHttpFetcherTestDelegate*>(data);
  if (delegate->once_) {
    delegate->TerminateTransfer();
    return TRUE;
  } else {
    delegate->EndLoop();
    return FALSE;
  }
}
}  // namespace {}

TYPED_TEST(HttpFetcherTest, AbortTest) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    AbortingHttpFetcherTestDelegate delegate;
    delegate.fetcher_.reset(this->test_.NewLargeFetcher());
    delegate.once_ = true;
    delegate.callback_once_ = true;
    delegate.loop_ = loop;
    delegate.fetcher_->set_delegate(&delegate);

    MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
        delegate.fetcher_->GetSystemState()->connection_manager());
    EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetWifi),
                            SetArgumentPointee<2>(NetworkTethering::kUnknown),
                            Return(true)));
    EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetWifi, _))
      .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_cm, StringForConnectionType(kNetWifi))
      .WillRepeatedly(Return(shill::kTypeWifi));

    scoped_ptr<HttpServer> server(this->test_.CreateServer());
    this->test_.IgnoreServerAborting(server.get());
    ASSERT_TRUE(server->started_);

    GSource* timeout_source_;
    timeout_source_ = g_timeout_source_new(0);  // ms
    g_source_set_callback(timeout_source_, AbortingTimeoutCallback, &delegate,
                          NULL);
    g_source_attach(timeout_source_, NULL);
    delegate.fetcher_->BeginTransfer(this->test_.BigUrl(server->GetPort()));

    g_main_loop_run(loop);
    CHECK(!delegate.once_);
    CHECK(!delegate.callback_once_);
    g_source_destroy(timeout_source_);
  }
  g_main_loop_unref(loop);
}

namespace {
class FlakyHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    data.append(bytes, length);
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_TRUE(successful);
    EXPECT_EQ(kHttpResponsePartialContent, fetcher->http_response_code());
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  string data;
  GMainLoop* loop_;
};
}  // namespace {}

TYPED_TEST(HttpFetcherTest, FlakyTest) {
  if (this->test_.IsMock())
    return;
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    FlakyHttpFetcherTestDelegate delegate;
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(this->test_.NewSmallFetcher());
    fetcher->set_delegate(&delegate);

    MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
        fetcher->GetSystemState()->connection_manager());
    EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetWifi),
                            SetArgumentPointee<2>(NetworkTethering::kUnknown),
                            Return(true)));
    EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetWifi, _))
      .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_cm, StringForConnectionType(kNetWifi))
      .WillRepeatedly(Return(shill::kTypeWifi));


    scoped_ptr<HttpServer> server(this->test_.CreateServer());
    ASSERT_TRUE(server->started_);

    StartTransferArgs start_xfer_args = {
      fetcher.get(),
      LocalServerUrlForPath(server->GetPort(),
                            base::StringPrintf("/flaky/%d/%d/%d/%d", kBigLength,
                                               kFlakyTruncateLength,
                                               kFlakySleepEvery,
                                               kFlakySleepSecs))
    };

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);

    // verify the data we get back
    ASSERT_EQ(kBigLength, delegate.data.size());
    for (int i = 0; i < kBigLength; i += 10) {
      // Assert so that we don't flood the screen w/ EXPECT errors on failure.
      ASSERT_EQ(delegate.data.substr(i, 10), "abcdefghij");
    }
  }
  g_main_loop_unref(loop);
}

namespace {
class FailureHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  FailureHttpFetcherTestDelegate(PythonHttpServer* server)
      : loop_(NULL),
        server_(server) {}

  virtual ~FailureHttpFetcherTestDelegate() {
    if (server_) {
      LOG(INFO) << "Stopping server in destructor";
      delete server_;
      LOG(INFO) << "server stopped";
    }
  }

  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    if (server_) {
      LOG(INFO) << "Stopping server in ReceivedBytes";
      delete server_;
      LOG(INFO) << "server stopped";
      server_ = NULL;
    }
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_FALSE(successful);
    EXPECT_EQ(0, fetcher->http_response_code());
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  GMainLoop* loop_;
  PythonHttpServer* server_;
};
}  // namespace {}


TYPED_TEST(HttpFetcherTest, FailureTest) {
  if (this->test_.IsMock())
    return;
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    FailureHttpFetcherTestDelegate delegate(NULL);
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(this->test_.NewSmallFetcher());
    fetcher->set_delegate(&delegate);

    MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
        fetcher->GetSystemState()->connection_manager());
    EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetEthernet),
                            SetArgumentPointee<2>(NetworkTethering::kUnknown),
                            Return(true)));
    EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetEthernet, _))
      .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_cm, StringForConnectionType(kNetEthernet))
      .WillRepeatedly(Return(shill::kTypeEthernet));


    StartTransferArgs start_xfer_args = {
      fetcher.get(),
      LocalServerUrlForPath(0, this->test_.SmallUrl(0))
    };

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);

    // Exiting and testing happens in the delegate
  }
  g_main_loop_unref(loop);
}

TYPED_TEST(HttpFetcherTest, ServerDiesTest) {
  if (this->test_.IsMock())
    return;
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    FailureHttpFetcherTestDelegate delegate(new PythonHttpServer);
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(this->test_.NewSmallFetcher());
    fetcher->set_delegate(&delegate);

    // Don't allow connection to server by denying access over ethernet.
    MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
        fetcher->GetSystemState()->connection_manager());
    EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetEthernet),
                            SetArgumentPointee<2>(NetworkTethering::kUnknown),
                            Return(true)));
    EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetEthernet, _))
      .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_cm, StringForConnectionType(kNetEthernet))
      .WillRepeatedly(Return(shill::kTypeEthernet));

    StartTransferArgs start_xfer_args = {
      fetcher.get(),
      LocalServerUrlForPath(0,
                            base::StringPrintf("/flaky/%d/%d/%d/%d", kBigLength,
                                               kFlakyTruncateLength,
                                               kFlakySleepEvery,
                                               kFlakySleepSecs))
    };
    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);

    // Exiting and testing happens in the delegate
  }
  g_main_loop_unref(loop);
}

namespace {
const HttpResponseCode kRedirectCodes[] = {
  kHttpResponseMovedPermanently, kHttpResponseFound, kHttpResponseSeeOther,
  kHttpResponseTempRedirect
};

class RedirectHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  RedirectHttpFetcherTestDelegate(bool expected_successful)
      : expected_successful_(expected_successful) {}
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    data.append(bytes, length);
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_EQ(expected_successful_, successful);
    if (expected_successful_)
      EXPECT_EQ(kHttpResponseOk, fetcher->http_response_code());
    else {
      EXPECT_GE(fetcher->http_response_code(), kHttpResponseMovedPermanently);
      EXPECT_LE(fetcher->http_response_code(), kHttpResponseTempRedirect);
    }
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  bool expected_successful_;
  string data;
  GMainLoop* loop_;
};

// RedirectTest takes ownership of |http_fetcher|.
void RedirectTest(const HttpServer* server,
                  bool expected_successful,
                  const string& url,
                  HttpFetcher* http_fetcher) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    RedirectHttpFetcherTestDelegate delegate(expected_successful);
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(http_fetcher);
    fetcher->set_delegate(&delegate);

    MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
        fetcher->GetSystemState()->connection_manager());
    EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetEthernet),
                            SetArgumentPointee<2>(NetworkTethering::kUnknown),
                            Return(true)));
    EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetEthernet, _))
      .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_cm, StringForConnectionType(kNetEthernet))
      .WillRepeatedly(Return(shill::kTypeEthernet));

    StartTransferArgs start_xfer_args =
        { fetcher.get(), LocalServerUrlForPath(server->GetPort(), url) };

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);
    if (expected_successful) {
      // verify the data we get back
      ASSERT_EQ(kMediumLength, delegate.data.size());
      for (int i = 0; i < kMediumLength; i += 10) {
        // Assert so that we don't flood the screen w/ EXPECT errors on failure.
        ASSERT_EQ(delegate.data.substr(i, 10), "abcdefghij");
      }
    }
  }
  g_main_loop_unref(loop);
}
}  // namespace {}

TYPED_TEST(HttpFetcherTest, SimpleRedirectTest) {
  if (this->test_.IsMock())
    return;

  scoped_ptr<HttpServer> server(this->test_.CreateServer());
  ASSERT_TRUE(server->started_);

  for (size_t c = 0; c < arraysize(kRedirectCodes); ++c) {
    const string url = base::StringPrintf("/redirect/%d/download/%d",
                                          kRedirectCodes[c],
                                          kMediumLength);
    RedirectTest(server.get(), true, url, this->test_.NewLargeFetcher());
  }
}

TYPED_TEST(HttpFetcherTest, MaxRedirectTest) {
  if (this->test_.IsMock())
    return;

  scoped_ptr<HttpServer> server(this->test_.CreateServer());
  ASSERT_TRUE(server->started_);

  string url;
  for (int r = 0; r < kDownloadMaxRedirects; r++) {
    url += base::StringPrintf("/redirect/%d",
                              kRedirectCodes[r % arraysize(kRedirectCodes)]);
  }
  url += base::StringPrintf("/download/%d", kMediumLength);
  RedirectTest(server.get(), true, url, this->test_.NewLargeFetcher());
}

TYPED_TEST(HttpFetcherTest, BeyondMaxRedirectTest) {
  if (this->test_.IsMock())
    return;

  scoped_ptr<HttpServer> server(this->test_.CreateServer());
  ASSERT_TRUE(server->started_);

  string url;
  for (int r = 0; r < kDownloadMaxRedirects + 1; r++) {
    url += base::StringPrintf("/redirect/%d",
                              kRedirectCodes[r % arraysize(kRedirectCodes)]);
  }
  url += base::StringPrintf("/download/%d", kMediumLength);
  RedirectTest(server.get(), false, url, this->test_.NewLargeFetcher());
}

namespace {
class MultiHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  MultiHttpFetcherTestDelegate(int expected_response_code)
      : expected_response_code_(expected_response_code) {}

  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    EXPECT_EQ(fetcher, fetcher_.get());
    data.append(bytes, length);
  }

  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_EQ(fetcher, fetcher_.get());
    EXPECT_EQ(expected_response_code_ != kHttpResponseUndefined, successful);
    if (expected_response_code_ != 0)
      EXPECT_EQ(expected_response_code_, fetcher->http_response_code());
    // Destroy the fetcher (because we're allowed to).
    fetcher_.reset(NULL);
    g_main_loop_quit(loop_);
  }

  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }

  scoped_ptr<HttpFetcher> fetcher_;
  int expected_response_code_;
  string data;
  GMainLoop* loop_;
};

void MultiTest(HttpFetcher* fetcher_in,
               const string& url,
               const vector<pair<off_t, off_t> >& ranges,
               const string& expected_prefix,
               off_t expected_size,
               HttpResponseCode expected_response_code) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    MultiHttpFetcherTestDelegate delegate(expected_response_code);
    delegate.loop_ = loop;
    delegate.fetcher_.reset(fetcher_in);

    MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
        fetcher_in->GetSystemState()->connection_manager());
    EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetWifi),
                            SetArgumentPointee<2>(NetworkTethering::kUnknown),
                            Return(true)));
    EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetWifi, _))
      .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_cm, StringForConnectionType(kNetWifi))
      .WillRepeatedly(Return(shill::kTypeWifi));

    MultiRangeHttpFetcher* multi_fetcher =
        dynamic_cast<MultiRangeHttpFetcher*>(fetcher_in);
    ASSERT_TRUE(multi_fetcher);
    multi_fetcher->ClearRanges();
    for (vector<pair<off_t, off_t> >::const_iterator it = ranges.begin(),
             e = ranges.end(); it != e; ++it) {
      std::string tmp_str = base::StringPrintf("%jd+", it->first);
      if (it->second > 0) {
        base::StringAppendF(&tmp_str, "%jd", it->second);
        multi_fetcher->AddRange(it->first, it->second);
      } else {
        base::StringAppendF(&tmp_str, "?");
        multi_fetcher->AddRange(it->first);
      }
      LOG(INFO) << "added range: " << tmp_str;
    }
    dynamic_cast<MockSystemState*>(fetcher_in->GetSystemState())
        ->fake_hardware()->SetIsOfficialBuild(false);
    multi_fetcher->set_delegate(&delegate);

    StartTransferArgs start_xfer_args = {multi_fetcher, url};

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);

    EXPECT_EQ(expected_size, delegate.data.size());
    EXPECT_EQ(expected_prefix,
              string(delegate.data.data(), expected_prefix.size()));
  }
  g_main_loop_unref(loop);
}
}  // namespace {}

TYPED_TEST(HttpFetcherTest, MultiHttpFetcherSimpleTest) {
  if (!this->test_.IsMulti())
    return;

  scoped_ptr<HttpServer> server(this->test_.CreateServer());
  ASSERT_TRUE(server->started_);

  vector<pair<off_t, off_t> > ranges;
  ranges.push_back(make_pair(0, 25));
  ranges.push_back(make_pair(99, 0));
  MultiTest(this->test_.NewLargeFetcher(),
            this->test_.BigUrl(server->GetPort()),
            ranges,
            "abcdefghijabcdefghijabcdejabcdefghijabcdef",
            kBigLength - (99 - 25),
            kHttpResponsePartialContent);
}

TYPED_TEST(HttpFetcherTest, MultiHttpFetcherLengthLimitTest) {
  if (!this->test_.IsMulti())
    return;

  scoped_ptr<HttpServer> server(this->test_.CreateServer());
  ASSERT_TRUE(server->started_);

  vector<pair<off_t, off_t> > ranges;
  ranges.push_back(make_pair(0, 24));
  MultiTest(this->test_.NewLargeFetcher(),
            this->test_.BigUrl(server->GetPort()),
            ranges,
            "abcdefghijabcdefghijabcd",
            24,
            kHttpResponsePartialContent);
}

TYPED_TEST(HttpFetcherTest, MultiHttpFetcherMultiEndTest) {
  if (!this->test_.IsMulti())
    return;

  scoped_ptr<HttpServer> server(this->test_.CreateServer());
  ASSERT_TRUE(server->started_);

  vector<pair<off_t, off_t> > ranges;
  ranges.push_back(make_pair(kBigLength - 2, 0));
  ranges.push_back(make_pair(kBigLength - 3, 0));
  MultiTest(this->test_.NewLargeFetcher(),
            this->test_.BigUrl(server->GetPort()),
            ranges,
            "ijhij",
            5,
            kHttpResponsePartialContent);
}

TYPED_TEST(HttpFetcherTest, MultiHttpFetcherInsufficientTest) {
  if (!this->test_.IsMulti())
    return;

  scoped_ptr<HttpServer> server(this->test_.CreateServer());
  ASSERT_TRUE(server->started_);

  vector<pair<off_t, off_t> > ranges;
  ranges.push_back(make_pair(kBigLength - 2, 4));
  for (int i = 0; i < 2; ++i) {
    LOG(INFO) << "i = " << i;
    MultiTest(this->test_.NewLargeFetcher(),
              this->test_.BigUrl(server->GetPort()),
              ranges,
              "ij",
              2,
              kHttpResponseUndefined);
    ranges.push_back(make_pair(0, 5));
  }
}

// Issue #18143: when a fetch of a secondary chunk out of a chain, then it
// should retry with other proxies listed before giving up.
//
// (1) successful recovery: The offset fetch will fail twice but succeed with
// the third proxy.
TYPED_TEST(HttpFetcherTest, MultiHttpFetcherErrorIfOffsetRecoverableTest) {
  if (!this->test_.IsMulti())
    return;

  scoped_ptr<HttpServer> server(this->test_.CreateServer());
  ASSERT_TRUE(server->started_);

  vector<pair<off_t, off_t> > ranges;
  ranges.push_back(make_pair(0, 25));
  ranges.push_back(make_pair(99, 0));
  MultiTest(this->test_.NewLargeFetcher(3),
            LocalServerUrlForPath(server->GetPort(),
                                  base::StringPrintf("/error-if-offset/%d/2",
                                                     kBigLength)),
            ranges,
            "abcdefghijabcdefghijabcdejabcdefghijabcdef",
            kBigLength - (99 - 25),
            kHttpResponsePartialContent);
}

// (2) unsuccessful recovery: The offset fetch will fail repeatedly.  The
// fetcher will signal a (failed) completed transfer to the delegate.
TYPED_TEST(HttpFetcherTest, MultiHttpFetcherErrorIfOffsetUnrecoverableTest) {
  if (!this->test_.IsMulti())
    return;

  scoped_ptr<HttpServer> server(this->test_.CreateServer());
  ASSERT_TRUE(server->started_);

  vector<pair<off_t, off_t> > ranges;
  ranges.push_back(make_pair(0, 25));
  ranges.push_back(make_pair(99, 0));
  MultiTest(this->test_.NewLargeFetcher(2),
            LocalServerUrlForPath(server->GetPort(),
                                  base::StringPrintf("/error-if-offset/%d/3",
                                                     kBigLength)),
            ranges,
            "abcdefghijabcdefghijabcde",  // only received the first chunk
            25,
            kHttpResponseUndefined);
}



namespace {
class BlockedTransferTestDelegate : public HttpFetcherDelegate {
 public:
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    ADD_FAILURE();
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_FALSE(successful);
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  GMainLoop* loop_;
};

}  // namespace

TYPED_TEST(HttpFetcherTest, BlockedTransferTest) {
  if (this->test_.IsMock() || this->test_.IsMulti())
    return;

  for (int i = 0; i < 2; i++) {
    scoped_ptr<HttpServer> server(this->test_.CreateServer());
    ASSERT_TRUE(server->started_);

    GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
    {
      BlockedTransferTestDelegate delegate;
      delegate.loop_ = loop;

      bool is_allowed = (i != 0);
      scoped_ptr<HttpFetcher> fetcher(this->test_.NewLargeFetcher());
      MockConnectionManager* mock_cm = dynamic_cast<MockConnectionManager*>(
          fetcher->GetSystemState()->connection_manager());
      EXPECT_CALL(*mock_cm, GetConnectionProperties(_,_,_))
        .WillRepeatedly(DoAll(SetArgumentPointee<1>(kNetWifi),
                              SetArgumentPointee<2>(NetworkTethering::kUnknown),
                              Return(true)));
      EXPECT_CALL(*mock_cm, IsUpdateAllowedOver(kNetWifi, _))
        .WillRepeatedly(Return(is_allowed));
      EXPECT_CALL(*mock_cm, StringForConnectionType(kNetWifi))
        .WillRepeatedly(Return(shill::kTypeWifi));

      bool is_official_build = (i == 1);
      LOG(INFO) << "is_update_allowed_over_connection: " << is_allowed;
      LOG(INFO) << "is_official_build: " << is_official_build;
      // NewLargeFetcher creates the HttpFetcher* with a MockSystemState.
      dynamic_cast<MockSystemState*>(fetcher->GetSystemState())
          ->fake_hardware()->SetIsOfficialBuild(is_official_build);
      fetcher->set_delegate(&delegate);

      StartTransferArgs start_xfer_args =
          {fetcher.get(),
           LocalServerUrlForPath(server->GetPort(),
                                 this->test_.SmallUrl(server->GetPort()))};

      g_timeout_add(0, StartTransfer, &start_xfer_args);
      g_main_loop_run(loop);
    }
    g_main_loop_unref(loop);
  }
}

}  // namespace chromeos_update_engine
