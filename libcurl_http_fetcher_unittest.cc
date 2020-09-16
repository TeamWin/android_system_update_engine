//
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/libcurl_http_fetcher.h"

#include <string>

#include <brillo/message_loops/fake_message_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "update_engine/common/fake_hardware.h"
#include "update_engine/common/mock_proxy_resolver.h"
#include "update_engine/mock_libcurl_http_fetcher.h"

using std::string;

namespace chromeos_update_engine {

namespace {
constexpr char kHeaderName[] = "X-Goog-Test-Header";
}

class LibcurlHttpFetcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_.SetAsCurrent();
    fake_hardware_.SetIsOfficialBuild(true);
    fake_hardware_.SetIsOOBEEnabled(false);
  }

  brillo::FakeMessageLoop loop_{nullptr};
  FakeHardware fake_hardware_;
  MockLibcurlHttpFetcher libcurl_fetcher_{nullptr, &fake_hardware_};
  UnresolvedHostStateMachine state_machine_;
};

TEST_F(LibcurlHttpFetcherTest, GetEmptyHeaderValueTest) {
  const string header_value = "";
  string actual_header_value;
  libcurl_fetcher_.SetHeader(kHeaderName, header_value);
  EXPECT_TRUE(libcurl_fetcher_.GetHeader(kHeaderName, &actual_header_value));
  EXPECT_EQ("", actual_header_value);
}

TEST_F(LibcurlHttpFetcherTest, GetHeaderTest) {
  const string header_value = "This-is-value 123";
  string actual_header_value;
  libcurl_fetcher_.SetHeader(kHeaderName, header_value);
  EXPECT_TRUE(libcurl_fetcher_.GetHeader(kHeaderName, &actual_header_value));
  EXPECT_EQ(header_value, actual_header_value);
}

TEST_F(LibcurlHttpFetcherTest, GetNonExistentHeaderValueTest) {
  string actual_header_value;
  // Skip |SetHeaader()| call.
  EXPECT_FALSE(libcurl_fetcher_.GetHeader(kHeaderName, &actual_header_value));
  // Even after a failed |GetHeaderValue()|, enforce that the passed pointer to
  // modifiable string was cleared to be empty.
  EXPECT_EQ("", actual_header_value);
}

TEST_F(LibcurlHttpFetcherTest, GetHeaderEdgeCaseTest) {
  const string header_value = "\a\b\t\v\f\r\\ edge:-case: \a\b\t\v\f\r\\";
  string actual_header_value;
  libcurl_fetcher_.SetHeader(kHeaderName, header_value);
  EXPECT_TRUE(libcurl_fetcher_.GetHeader(kHeaderName, &actual_header_value));
  EXPECT_EQ(header_value, actual_header_value);
}

TEST_F(LibcurlHttpFetcherTest, InvalidURLTest) {
  int no_network_max_retries = 1;
  libcurl_fetcher_.set_no_network_max_retries(no_network_max_retries);

  libcurl_fetcher_.BeginTransfer("not-a-URL");
  while (loop_.PendingTasks()) {
    loop_.RunOnce(true);
  }

  EXPECT_EQ(libcurl_fetcher_.get_no_network_max_retries(),
            no_network_max_retries);
}

TEST_F(LibcurlHttpFetcherTest, CouldNotResolveHostTest) {
  int no_network_max_retries = 1;
  libcurl_fetcher_.set_no_network_max_retries(no_network_max_retries);

  libcurl_fetcher_.BeginTransfer("https://An-uNres0lvable-uRl.invalid");

#ifdef __ANDROID__
  // It's slower on Android that libcurl handle may not finish within 1 cycle.
  // Will need to wait for more cycles until it finishes. Original test didn't
  // correctly handle when we need to re-watch libcurl fds.
  while (loop_.PendingTasks() &&
         libcurl_fetcher_.GetAuxiliaryErrorCode() == ErrorCode::kSuccess) {
    loop_.RunOnce(true);
  }
#else
  // The first time it can't resolve.
  loop_.RunOnce(true);
#endif
  EXPECT_EQ(libcurl_fetcher_.GetAuxiliaryErrorCode(),
            ErrorCode::kUnresolvedHostError);

  while (loop_.PendingTasks()) {
    loop_.RunOnce(true);
  }
  // The auxilary error code should've have been changed.
  EXPECT_EQ(libcurl_fetcher_.GetAuxiliaryErrorCode(),
            ErrorCode::kUnresolvedHostError);

  // If libcurl fails to resolve the name, we call res_init() to reload
  // resolv.conf and retry exactly once more. See crbug.com/982813 for details.
  EXPECT_EQ(libcurl_fetcher_.get_no_network_max_retries(),
            no_network_max_retries + 1);
}

TEST_F(LibcurlHttpFetcherTest, HostResolvedTest) {
  int no_network_max_retries = 2;
  libcurl_fetcher_.set_no_network_max_retries(no_network_max_retries);

  // This test actually sends request to internet but according to
  // https://tools.ietf.org/html/rfc2606#section-2, .invalid domain names are
  // reserved and sure to be invalid. Ideally we should mock libcurl or
  // reorganize LibcurlHttpFetcher so the part that sends request can be mocked
  // easily.
  // TODO(xiaochu) Refactor LibcurlHttpFetcher (and its relates) so it's
  // easier to mock the part that depends on internet connectivity.
  libcurl_fetcher_.BeginTransfer("https://An-uNres0lvable-uRl.invalid");

#ifdef __ANDROID__
  // It's slower on Android that libcurl handle may not finish within 1 cycle.
  // Will need to wait for more cycles until it finishes. Original test didn't
  // correctly handle when we need to re-watch libcurl fds.
  while (loop_.PendingTasks() &&
         libcurl_fetcher_.GetAuxiliaryErrorCode() == ErrorCode::kSuccess) {
    loop_.RunOnce(true);
  }
#else
  // The first time it can't resolve.
  loop_.RunOnce(true);
#endif
  EXPECT_EQ(libcurl_fetcher_.GetAuxiliaryErrorCode(),
            ErrorCode::kUnresolvedHostError);

  // The second time, it will resolve, with error code 200 but we set the
  // download size be smaller than the transfer size so it will retry again.
  EXPECT_CALL(libcurl_fetcher_, GetHttpResponseCode())
      .WillOnce(testing::Invoke(
          [this]() { libcurl_fetcher_.http_response_code_ = 200; }))
      .WillRepeatedly(testing::Invoke(
          [this]() { libcurl_fetcher_.http_response_code_ = 0; }));
  libcurl_fetcher_.transfer_size_ = 10;

#ifdef __ANDROID__
  // It's slower on Android that libcurl handle may not finish within 1 cycle.
  // Will need to wait for more cycles until it finishes. Original test didn't
  // correctly handle when we need to re-watch libcurl fds.
  while (loop_.PendingTasks() && libcurl_fetcher_.GetAuxiliaryErrorCode() ==
                                     ErrorCode::kUnresolvedHostError) {
    loop_.RunOnce(true);
  }
#else
  // This time the host is resolved. But after that again we can't resolve
  // anymore (See above).
  loop_.RunOnce(true);
#endif
  EXPECT_EQ(libcurl_fetcher_.GetAuxiliaryErrorCode(),
            ErrorCode::kUnresolvedHostRecovered);

  while (loop_.PendingTasks()) {
    loop_.RunOnce(true);
  }
  // The auxilary error code should not have been changed.
  EXPECT_EQ(libcurl_fetcher_.GetAuxiliaryErrorCode(),
            ErrorCode::kUnresolvedHostRecovered);

  // If libcurl fails to resolve the name, we call res_init() to reload
  // resolv.conf and retry exactly once more. See crbug.com/982813 for details.
  EXPECT_EQ(libcurl_fetcher_.get_no_network_max_retries(),
            no_network_max_retries + 1);
}

TEST_F(LibcurlHttpFetcherTest, HttpFetcherStateMachineRetryFailedTest) {
  state_machine_.UpdateState(true);
  state_machine_.UpdateState(true);
  EXPECT_EQ(state_machine_.GetState(),
            UnresolvedHostStateMachine::State::kNotRetry);
}

TEST_F(LibcurlHttpFetcherTest, HttpFetcherStateMachineRetrySucceedTest) {
  state_machine_.UpdateState(true);
  state_machine_.UpdateState(false);
  EXPECT_EQ(state_machine_.GetState(),
            UnresolvedHostStateMachine::State::kRetriedSuccess);
}

TEST_F(LibcurlHttpFetcherTest, HttpFetcherStateMachineNoRetryTest) {
  state_machine_.UpdateState(false);
  state_machine_.UpdateState(false);
  EXPECT_EQ(state_machine_.GetState(),
            UnresolvedHostStateMachine::State::kInit);
}

}  // namespace chromeos_update_engine
