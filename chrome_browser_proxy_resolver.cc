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

#include <utility>

#include <base/bind.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_tokenizer.h>
#include <base/strings/string_util.h>

#include "network_proxy/dbus-proxies.h"

namespace chromeos_update_engine {

using base::StringTokenizer;
using std::deque;
using std::string;

namespace {

// Timeout for D-Bus calls in milliseconds.
constexpr int kTimeoutMs = 5000;

}  // namespace

ChromeBrowserProxyResolver::ChromeBrowserProxyResolver(
    org::chromium::NetworkProxyServiceInterfaceProxyInterface* dbus_proxy)
    : dbus_proxy_(dbus_proxy),
      next_request_id_(kProxyRequestIdNull + 1),
      weak_ptr_factory_(this) {}

ChromeBrowserProxyResolver::~ChromeBrowserProxyResolver() = default;

// static
deque<string> ChromeBrowserProxyResolver::ParseProxyString(
    const string& input) {
  deque<string> ret;
  // Some of this code taken from
  // http://src.chromium.org/svn/trunk/src/net/proxy/proxy_server.cc and
  // http://src.chromium.org/svn/trunk/src/net/proxy/proxy_list.cc
  StringTokenizer entry_tok(input, ";");
  while (entry_tok.GetNext()) {
    string token = entry_tok.token();
    base::TrimWhitespaceASCII(token, base::TRIM_ALL, &token);

    // Start by finding the first space (if any).
    string::iterator space;
    for (space = token.begin(); space != token.end(); ++space) {
      if (base::IsAsciiWhitespace(*space)) {
        break;
      }
    }

    string scheme = base::ToLowerASCII(string(token.begin(), space));
    // Chrome uses "socks" to mean socks4 and "proxy" to mean http.
    if (scheme == "socks")
      scheme += "4";
    else if (scheme == "proxy")
      scheme = "http";
    else if (scheme != "https" &&
             scheme != "socks4" &&
             scheme != "socks5" &&
             scheme != "direct")
      continue;  // Invalid proxy scheme

    string host_and_port = string(space, token.end());
    base::TrimWhitespaceASCII(host_and_port, base::TRIM_ALL, &host_and_port);
    if (scheme != "direct" && host_and_port.empty())
      continue;  // Must supply host/port when non-direct proxy used.
    ret.push_back(scheme + "://" + host_and_port);
  }
  if (ret.empty() || *ret.rbegin() != kNoProxy)
    ret.push_back(kNoProxy);
  return ret;
}

ProxyRequestId ChromeBrowserProxyResolver::GetProxiesForUrl(
    const string& url, const ProxiesResolvedFn& callback) {
  const ProxyRequestId id = next_request_id_++;
  dbus_proxy_->ResolveProxyAsync(
      url,
      base::Bind(&ChromeBrowserProxyResolver::OnResolveProxyResponse,
                 weak_ptr_factory_.GetWeakPtr(), id),
      base::Bind(&ChromeBrowserProxyResolver::OnResolveProxyError,
                 weak_ptr_factory_.GetWeakPtr(), id),
      kTimeoutMs);
  pending_callbacks_[id] = callback;
  return id;
}

bool ChromeBrowserProxyResolver::CancelProxyRequest(ProxyRequestId request) {
  return pending_callbacks_.erase(request) != 0;
}

void ChromeBrowserProxyResolver::OnResolveProxyResponse(
    ProxyRequestId request_id,
    const std::string& proxy_info,
    const std::string& error_message) {
  if (!error_message.empty())
    LOG(WARNING) << "Got error resolving proxy: " << error_message;
  RunCallback(request_id, ParseProxyString(proxy_info));
}

void ChromeBrowserProxyResolver::OnResolveProxyError(ProxyRequestId request_id,
                                                     brillo::Error* error) {
  LOG(WARNING) << "Failed to resolve proxy: "
               << (error ? error->GetMessage() : "[null]");
  RunCallback(request_id, deque<string>{kNoProxy});
}

void ChromeBrowserProxyResolver::RunCallback(
    ProxyRequestId request_id,
    const std::deque<std::string>& proxies) {
  auto it = pending_callbacks_.find(request_id);
  if (it == pending_callbacks_.end())
    return;

  ProxiesResolvedFn callback = it->second;
  pending_callbacks_.erase(it);
  callback.Run(proxies);
}

} // namespace chromeos_update_engine
