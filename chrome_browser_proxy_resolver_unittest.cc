// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/chrome_browser_proxy_resolver.h"

#include <deque>
#include <string>

#include <gtest/gtest.h>

#include <base/bind.h>
#include <chromeos/message_loops/fake_message_loop.h>

#include "update_engine/mock_dbus_wrapper.h"

using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::_;
using chromeos::MessageLoop;
using std::deque;
using std::string;

namespace chromeos_update_engine {

class ChromeBrowserProxyResolverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_.SetAsCurrent();
  }

  void TearDown() override {
    EXPECT_FALSE(loop_.PendingTasks());
  }

 private:
  chromeos::FakeMessageLoop loop_{nullptr};
};

TEST_F(ChromeBrowserProxyResolverTest, ParseTest) {
  // Test ideas from
  // http://src.chromium.org/svn/trunk/src/net/proxy/proxy_list_unittest.cc
  const char* inputs[] = {
    "PROXY foopy:10",
    " DIRECT",  // leading space.
    "PROXY foopy1 ; proxy foopy2;\t DIRECT",
    "proxy foopy1 ; SOCKS foopy2",
    "DIRECT ; proxy foopy1 ; DIRECT ; SOCKS5 foopy2;DIRECT ",
    "DIRECT ; proxy foopy1:80; DIRECT ; DIRECT",
    "PROXY-foopy:10",
    "PROXY",
    "PROXY foopy1 ; JUNK ; JUNK ; SOCKS5 foopy2 ; ;",
    "HTTP foopy1; SOCKS5 foopy2"
  };
  deque<string> outputs[arraysize(inputs)];
  outputs[0].push_back("http://foopy:10");
  outputs[0].push_back(kNoProxy);
  outputs[1].push_back(kNoProxy);
  outputs[2].push_back("http://foopy1");
  outputs[2].push_back("http://foopy2");
  outputs[2].push_back(kNoProxy);
  outputs[3].push_back("http://foopy1");
  outputs[3].push_back("socks4://foopy2");
  outputs[3].push_back(kNoProxy);
  outputs[4].push_back(kNoProxy);
  outputs[4].push_back("http://foopy1");
  outputs[4].push_back(kNoProxy);
  outputs[4].push_back("socks5://foopy2");
  outputs[4].push_back(kNoProxy);
  outputs[5].push_back(kNoProxy);
  outputs[5].push_back("http://foopy1:80");
  outputs[5].push_back(kNoProxy);
  outputs[5].push_back(kNoProxy);
  outputs[6].push_back(kNoProxy);
  outputs[7].push_back(kNoProxy);
  outputs[8].push_back("http://foopy1");
  outputs[8].push_back("socks5://foopy2");
  outputs[8].push_back(kNoProxy);
  outputs[9].push_back("socks5://foopy2");
  outputs[9].push_back(kNoProxy);

  for (size_t i = 0; i < arraysize(inputs); i++) {
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

namespace {
void DBusWrapperTestResolved(const deque<string>& proxies,
                             void* /* pirv_data */) {
  EXPECT_EQ(2, proxies.size());
  EXPECT_EQ("socks5://192.168.52.83:5555", proxies[0]);
  EXPECT_EQ(kNoProxy, proxies[1]);
  MessageLoop::current()->BreakLoop();
}
void DBusWrapperTestResolvedNoReply(const deque<string>& proxies,
                                    void* /* pirv_data */) {
  EXPECT_EQ(1, proxies.size());
  EXPECT_EQ(kNoProxy, proxies[0]);
  MessageLoop::current()->BreakLoop();
}

void SendReply(DBusConnection* connection,
               DBusMessage* message,
               ChromeBrowserProxyResolver* resolver) {
  LOG(INFO) << "Calling SendReply";
  ChromeBrowserProxyResolver::StaticFilterMessage(connection,
                                                  message,
                                                  resolver);
}

// chrome_replies should be set to whether or not we fake a reply from
// chrome. If there's no reply, the resolver should time out.
// If chrome_alive is false, assume that sending to chrome fails.
void RunTest(bool chrome_replies, bool chrome_alive) {
  intptr_t number = 1;
  DBusGConnection* kMockSystemGBus =
      reinterpret_cast<DBusGConnection*>(number++);
  DBusConnection* kMockSystemBus =
      reinterpret_cast<DBusConnection*>(number++);
  DBusGProxy* kMockDbusProxy =
      reinterpret_cast<DBusGProxy*>(number++);
  DBusMessage* kMockDbusMessage =
      reinterpret_cast<DBusMessage*>(number++);

  char kUrl[] = "http://example.com/blah";
  char kProxyConfig[] = "SOCKS5 192.168.52.83:5555;DIRECT";

  testing::StrictMock<MockDBusWrapper> dbus_iface;

  EXPECT_CALL(dbus_iface, BusGet(_, _))
      .Times(2)
      .WillRepeatedly(Return(kMockSystemGBus));
  EXPECT_CALL(dbus_iface,
              ConnectionGetConnection(kMockSystemGBus))
      .Times(2)
      .WillRepeatedly(Return(kMockSystemBus));
  EXPECT_CALL(dbus_iface, DBusBusAddMatch(kMockSystemBus, _, _));
  EXPECT_CALL(dbus_iface,
              DBusConnectionAddFilter(kMockSystemBus, _, _, _))
      .WillOnce(Return(1));
  EXPECT_CALL(dbus_iface,
              ProxyNewForName(kMockSystemGBus,
                              StrEq(kLibCrosServiceName),
                              StrEq(kLibCrosServicePath),
                              StrEq(kLibCrosServiceInterface)))
      .WillOnce(Return(kMockDbusProxy));
  EXPECT_CALL(dbus_iface, ProxyUnref(kMockDbusProxy));

  EXPECT_CALL(dbus_iface, ProxyCall_3_0(
      kMockDbusProxy,
      StrEq(kLibCrosServiceResolveNetworkProxyMethodName),
      _,
      StrEq(kUrl),
      StrEq(kLibCrosProxyResolveSignalInterface),
      StrEq(kLibCrosProxyResolveName)))
      .WillOnce(Return(chrome_alive ? TRUE : FALSE));

  EXPECT_CALL(dbus_iface,
              DBusConnectionRemoveFilter(kMockSystemBus, _, _));

  if (chrome_replies) {
    EXPECT_CALL(dbus_iface,
                DBusMessageIsSignal(kMockDbusMessage,
                                    kLibCrosProxyResolveSignalInterface,
                                    kLibCrosProxyResolveName))
        .WillOnce(Return(1));
    EXPECT_CALL(dbus_iface,
                DBusMessageGetArgs_3(kMockDbusMessage, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(static_cast<char*>(kUrl)),
                        SetArgPointee<3>(static_cast<char*>(kProxyConfig)),
                        Return(TRUE)));
  }

  ChromeBrowserProxyResolver resolver(&dbus_iface);
  EXPECT_EQ(true, resolver.Init());
  resolver.set_timeout(1);
  ProxiesResolvedFn get_proxies_response = &DBusWrapperTestResolvedNoReply;

  if (chrome_replies) {
    get_proxies_response = &DBusWrapperTestResolved;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SendReply, kMockSystemBus, kMockDbusMessage, &resolver));
  }

  EXPECT_TRUE(resolver.GetProxiesForUrl(kUrl, get_proxies_response, nullptr));
  MessageLoop::current()->Run();
}
}  // namespace

TEST_F(ChromeBrowserProxyResolverTest, SuccessTest) {
  RunTest(true, true);
}

TEST_F(ChromeBrowserProxyResolverTest, NoReplyTest) {
  RunTest(false, true);
}

TEST_F(ChromeBrowserProxyResolverTest, NoChromeTest) {
  RunTest(false, false);
}

}  // namespace chromeos_update_engine
