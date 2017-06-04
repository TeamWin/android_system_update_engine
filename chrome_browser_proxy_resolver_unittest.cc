//
// Copyright (C) 2011 The Android Open Source Project
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

#include "update_engine/chrome_browser_proxy_resolver.h"

#include <deque>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <base/macros.h>
#include <brillo/errors/error.h>

#include "network_proxy/dbus-proxies.h"
#include "network_proxy/dbus-proxy-mocks.h"
#include "update_engine/dbus_test_utils.h"

using ::testing::DoAll;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::_;
using org::chromium::NetworkProxyServiceInterfaceProxyMock;
using std::deque;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

// Callback for ProxyResolver::GetProxiesForUrl() that copies |src| to |dest|.
void CopyProxies(deque<string>* dest, const deque<string>& src) {
  *dest = src;
}

}  // namespace

class ChromeBrowserProxyResolverTest : public ::testing::Test {
 public:
  ChromeBrowserProxyResolverTest() = default;
  ~ChromeBrowserProxyResolverTest() override = default;

 protected:
  // Adds a GoogleMock expectation for a call to |dbus_proxy_|'s
  // ResolveProxyAsync method to resolve |url|.
  void AddResolveProxyExpectation(const std::string& url) {
    EXPECT_CALL(dbus_proxy_, ResolveProxyAsync(StrEq(url), _, _, _))
        .WillOnce(DoAll(SaveArg<1>(&success_callback_),
                        SaveArg<2>(&error_callback_)));
  }

  NetworkProxyServiceInterfaceProxyMock dbus_proxy_;
  ChromeBrowserProxyResolver resolver_{&dbus_proxy_};

  // Callbacks that were passed to |dbus_proxy_|'s ResolveProxyAsync method.
  base::Callback<void(const std::string&, const std::string&)>
      success_callback_;
  base::Callback<void(brillo::Error*)> error_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserProxyResolverTest);
};

TEST_F(ChromeBrowserProxyResolverTest, Parse) {
  // Test ideas from
  // http://src.chromium.org/svn/trunk/src/net/proxy/proxy_list_unittest.cc
  vector<string> inputs = {
      "PROXY foopy:10",
      " DIRECT", // leading space.
      "PROXY foopy1 ; proxy foopy2;\t DIRECT",
      "proxy foopy1 ; SOCKS foopy2",
      "DIRECT ; proxy foopy1 ; DIRECT ; SOCKS5 foopy2;DIRECT ",
      "DIRECT ; proxy foopy1:80; DIRECT ; DIRECT",
      "PROXY-foopy:10",
      "PROXY",
      "PROXY foopy1 ; JUNK ; JUNK ; SOCKS5 foopy2 ; ;",
      "HTTP foopy1; SOCKS5 foopy2",
  };
  vector<deque<string>> outputs = {
      {"http://foopy:10", kNoProxy},
      {kNoProxy},
      {"http://foopy1", "http://foopy2", kNoProxy},
      {"http://foopy1", "socks4://foopy2", kNoProxy},
      {kNoProxy, "http://foopy1", kNoProxy, "socks5://foopy2", kNoProxy},
      {kNoProxy, "http://foopy1:80", kNoProxy, kNoProxy},
      {kNoProxy},
      {kNoProxy},
      {"http://foopy1", "socks5://foopy2", kNoProxy},
      {"socks5://foopy2", kNoProxy},
  };
  ASSERT_EQ(inputs.size(), outputs.size());

  for (size_t i = 0; i < inputs.size(); i++) {
    deque<string> results =
        ChromeBrowserProxyResolver::ParseProxyString(inputs[i]);
    deque<string>& expected = outputs[i];
    EXPECT_EQ(results.size(), expected.size()) << "i = " << i;
    if (expected.size() != results.size())
      continue;
    for (size_t j = 0; j < expected.size(); j++) {
      EXPECT_EQ(expected[j], results[j]) << "i = " << i;
    }
  }
}

TEST_F(ChromeBrowserProxyResolverTest, Success) {
  const char kUrl[] = "http://example.com/blah";
  const char kProxyConfig[] = "SOCKS5 192.168.52.83:5555;DIRECT";
  AddResolveProxyExpectation(kUrl);
  deque<string> proxies;
  resolver_.GetProxiesForUrl(kUrl, base::Bind(&CopyProxies, &proxies));

  // Run the D-Bus success callback and verify that the proxies are passed to
  // the supplied function.
  ASSERT_FALSE(success_callback_.is_null());
  success_callback_.Run(kProxyConfig, string());
  ASSERT_EQ(2u, proxies.size());
  EXPECT_EQ("socks5://192.168.52.83:5555", proxies[0]);
  EXPECT_EQ(kNoProxy, proxies[1]);
}

TEST_F(ChromeBrowserProxyResolverTest, Failure) {
  const char kUrl[] = "http://example.com/blah";
  AddResolveProxyExpectation(kUrl);
  deque<string> proxies;
  resolver_.GetProxiesForUrl(kUrl, base::Bind(&CopyProxies, &proxies));

  // Run the D-Bus error callback and verify that the supplied function is
  // instructed to use a direct connection.
  ASSERT_FALSE(error_callback_.is_null());
  brillo::ErrorPtr error = brillo::Error::Create(FROM_HERE, "", "", "");
  error_callback_.Run(error.get());
  ASSERT_EQ(1u, proxies.size());
  EXPECT_EQ(kNoProxy, proxies[0]);
}

TEST_F(ChromeBrowserProxyResolverTest, CancelCallback) {
  const char kUrl[] = "http://example.com/blah";
  AddResolveProxyExpectation(kUrl);
  int called = 0;
  auto callback = base::Bind(
      [](int* called, const deque<string>& proxies) { (*called)++; }, &called);
  ProxyRequestId request = resolver_.GetProxiesForUrl(kUrl, callback);

  // Cancel the request and then run the D-Bus success callback. The original
  // callback shouldn't be run.
  EXPECT_TRUE(resolver_.CancelProxyRequest(request));
  ASSERT_FALSE(success_callback_.is_null());
  success_callback_.Run("DIRECT", string());
  EXPECT_EQ(0, called);
}

TEST_F(ChromeBrowserProxyResolverTest, CancelCallbackTwice) {
  const char kUrl[] = "http://example.com/blah";
  AddResolveProxyExpectation(kUrl);
  deque<string> proxies;
  ProxyRequestId request =
      resolver_.GetProxiesForUrl(kUrl, base::Bind(&CopyProxies, &proxies));

  // Cancel the same request twice. The second call should fail.
  EXPECT_TRUE(resolver_.CancelProxyRequest(request));
  EXPECT_FALSE(resolver_.CancelProxyRequest(request));
}

}  // namespace chromeos_update_engine
