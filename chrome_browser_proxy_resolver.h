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
#include <map>
#include <string>

#include <base/memory/weak_ptr.h>

#include "update_engine/proxy_resolver.h"

namespace brillo {
class Error;
}  // namespace brillo

namespace org {
namespace chromium {
class NetworkProxyServiceInterfaceProxyInterface;
}  // namespace chromium
}  // namespace org

namespace chromeos_update_engine {

class ChromeBrowserProxyResolver : public ProxyResolver {
 public:
  explicit ChromeBrowserProxyResolver(
      org::chromium::NetworkProxyServiceInterfaceProxyInterface* dbus_proxy);
  ~ChromeBrowserProxyResolver() override;

  // Parses a string-encoded list of proxies and returns a deque
  // of individual proxies. The last one will always be kNoProxy.
  static std::deque<std::string> ParseProxyString(const std::string& input);

  // ProxyResolver:
  ProxyRequestId GetProxiesForUrl(const std::string& url,
                                  const ProxiesResolvedFn& callback) override;
  bool CancelProxyRequest(ProxyRequestId request) override;

private:
  // Callback for successful D-Bus calls made by GetProxiesForUrl().
  void OnResolveProxyResponse(ProxyRequestId request_id,
                              const std::string& proxy_info,
                              const std::string& error_message);

  // Callback for failed D-Bus calls made by GetProxiesForUrl().
  void OnResolveProxyError(ProxyRequestId request_id, brillo::Error* error);

  // Finds the callback identified by |request_id| in |pending_callbacks_|,
  // passes |proxies| to it, and deletes it. Does nothing if the request has
  // been cancelled.
  void RunCallback(ProxyRequestId request_id,
                   const std::deque<std::string>& proxies);

  // D-Bus proxy for resolving network proxies.
  org::chromium::NetworkProxyServiceInterfaceProxyInterface* dbus_proxy_;

  // Next ID to return from GetProxiesForUrl().
  ProxyRequestId next_request_id_;

  // Callbacks that were passed to GetProxiesForUrl() but haven't yet been run.
  std::map<ProxyRequestId, ProxiesResolvedFn> pending_callbacks_;

  base::WeakPtrFactory<ChromeBrowserProxyResolver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserProxyResolver);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CHROME_BROWSER_PROXY_RESOLVER_H_
