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
#include <gtest/gtest.h>

#include "update_engine/common/fake_hardware.h"
#include "update_engine/common/mock_proxy_resolver.h"

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
  LibcurlHttpFetcher libcurl_fetcher_{nullptr, &fake_hardware_};
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

}  // namespace chromeos_update_engine
