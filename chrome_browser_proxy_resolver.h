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

#ifndef UPDATE_ENGINE_CHROME_BROWSER_PROXY_RESOLVER_H_
#define UPDATE_ENGINE_CHROME_BROWSER_PROXY_RESOLVER_H_

#include <deque>
#include <string>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <brillo/message_loops/message_loop.h>

#include "update_engine/libcros_proxy.h"
#include "update_engine/proxy_resolver.h"

namespace chromeos_update_engine {

extern const char kLibCrosServiceName[];
extern const char kLibCrosProxyResolveName[];
extern const char kLibCrosProxyResolveSignalInterface[];

class ChromeBrowserProxyResolver : public ProxyResolver {
 public:
  explicit ChromeBrowserProxyResolver(LibCrosProxy* libcros_proxy);
  ~ChromeBrowserProxyResolver() override;

  // Initialize the ProxyResolver using the provided DBus proxies.
  bool Init();

  ProxyRequestId GetProxiesForUrl(const std::string& url,
                                  const ProxiesResolvedFn& callback) override;
  bool CancelProxyRequest(ProxyRequestId request) override;

 private:
  FRIEND_TEST(ChromeBrowserProxyResolverTest, ParseTest);
  FRIEND_TEST(ChromeBrowserProxyResolverTest, SuccessTest);
  struct ProxyRequestData {
    brillo::MessageLoop::TaskId timeout_id;
    ProxiesResolvedFn callback;
  };
  typedef std::multimap<std::string, std::unique_ptr<ProxyRequestData>>
      CallbacksMap;

  // Called when the signal in UpdateEngineLibcrosProxyResolvedInterface is
  // connected.
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool successful);

  // Handle a reply from Chrome:
  void OnProxyResolvedSignal(const std::string& source_url,
                             const std::string& proxy_info,
                             const std::string& error_message);

  // Handle no reply. The |request| pointer points to the ProxyRequestData in
  // the |callbacks_| map that triggered this timeout.
  void HandleTimeout(std::string source_url, ProxyRequestData* request);

  // Parses a string-encoded list of proxies and returns a deque
  // of individual proxies. The last one will always be kNoProxy.
  static std::deque<std::string> ParseProxyString(const std::string& input);

  // Process a proxy response by calling all the callbacks associated with the
  // passed |source_url|. All the timeouts associated with these callbacks will
  // be removed.
  void ProcessUrlResponse(const std::string& source_url,
                          const std::deque<std::string>& proxies);

  // Shutdown the dbus proxy object.
  void Shutdown();

  // DBus proxies to request a HTTP proxy resolution. The request is done in the
  // service_interface_proxy() interface and the response is received as a
  // signal in the ue_proxy_resolved_interface().
  LibCrosProxy* libcros_proxy_;

  int timeout_;
  CallbacksMap callbacks_;
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserProxyResolver);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CHROME_BROWSER_PROXY_RESOLVER_H_
