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

#include <base/bind.h>
#include <base/strings/string_tokenizer.h>
#include <base/strings/string_util.h>

#include "update_engine/common/utils.h"

namespace chromeos_update_engine {

using base::StringTokenizer;
using base::TimeDelta;
using brillo::MessageLoop;
using std::deque;
using std::string;

const char kLibCrosServiceName[] = "org.chromium.LibCrosService";
const char kLibCrosProxyResolveName[] = "ProxyResolved";
const char kLibCrosProxyResolveSignalInterface[] =
    "org.chromium.UpdateEngineLibcrosProxyResolvedInterface";

namespace {

const int kTimeout = 5;  // seconds

}  // namespace

ChromeBrowserProxyResolver::ChromeBrowserProxyResolver(
    LibCrosProxy* libcros_proxy)
    : libcros_proxy_(libcros_proxy), timeout_(kTimeout) {}

bool ChromeBrowserProxyResolver::Init() {
  libcros_proxy_->ue_proxy_resolved_interface()
      ->RegisterProxyResolvedSignalHandler(
          base::Bind(&ChromeBrowserProxyResolver::OnProxyResolvedSignal,
                     base::Unretained(this)),
          base::Bind(&ChromeBrowserProxyResolver::OnSignalConnected,
                     base::Unretained(this)));
  return true;
}

ChromeBrowserProxyResolver::~ChromeBrowserProxyResolver() {
  // Kill outstanding timers.
  for (const auto& it : callbacks_) {
    MessageLoop::current()->CancelTask(it.second->timeout_id);
  }
}

ProxyRequestId ChromeBrowserProxyResolver::GetProxiesForUrl(
    const string& url, const ProxiesResolvedFn& callback) {
  int timeout = timeout_;
  brillo::ErrorPtr error;
  if (!libcros_proxy_->service_interface_proxy()->ResolveNetworkProxy(
          url.c_str(),
          kLibCrosProxyResolveSignalInterface,
          kLibCrosProxyResolveName,
          &error)) {
    LOG(WARNING) << "Can't resolve the proxy. Continuing with no proxy.";
    timeout = 0;
  }

  std::unique_ptr<ProxyRequestData> request(new ProxyRequestData());
  request->callback = callback;
  ProxyRequestId timeout_id = MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ChromeBrowserProxyResolver::HandleTimeout,
                 base::Unretained(this),
                 url,
                 request.get()),
      TimeDelta::FromSeconds(timeout));
  request->timeout_id = timeout_id;
  callbacks_.emplace(url, std::move(request));

  // We re-use the timeout_id from the MessageLoop as the request id.
  return timeout_id;
}

bool ChromeBrowserProxyResolver::CancelProxyRequest(ProxyRequestId request) {
  // Finding the timeout_id in the callbacks_ structure requires a linear search
  // but we expect this operation to not be so frequent and to have just a few
  // proxy requests, so this should be fast enough.
  for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
    if (it->second->timeout_id == request) {
      MessageLoop::current()->CancelTask(request);
      callbacks_.erase(it);
      return true;
    }
  }
  return false;
}

void ChromeBrowserProxyResolver::ProcessUrlResponse(
    const string& source_url, const deque<string>& proxies) {
  // Call all the occurrences of the |source_url| and erase them.
  auto lower_end = callbacks_.lower_bound(source_url);
  auto upper_end = callbacks_.upper_bound(source_url);
  for (auto it = lower_end; it != upper_end; ++it) {
    ProxyRequestData* request = it->second.get();
    MessageLoop::current()->CancelTask(request->timeout_id);
    request->callback.Run(proxies);
  }
  callbacks_.erase(lower_end, upper_end);
}

void ChromeBrowserProxyResolver::OnSignalConnected(const string& interface_name,
                                                   const string& signal_name,
                                                   bool successful) {
  if (!successful) {
    LOG(ERROR) << "Couldn't connect to the signal " << interface_name << "."
               << signal_name;
  }
}

void ChromeBrowserProxyResolver::OnProxyResolvedSignal(
    const string& source_url,
    const string& proxy_info,
    const string& error_message) {
  if (!error_message.empty()) {
    LOG(WARNING) << "ProxyResolved error: " << error_message;
  }
  ProcessUrlResponse(source_url, ParseProxyString(proxy_info));
}

void ChromeBrowserProxyResolver::HandleTimeout(string source_url,
                                               ProxyRequestData* request) {
  LOG(INFO) << "Timeout handler called. Seems Chrome isn't responding.";
  // Mark the timer_id that produced this callback as invalid to prevent
  // canceling the timeout callback that already fired.
  request->timeout_id = MessageLoop::kTaskIdNull;

  deque<string> proxies = {kNoProxy};
  ProcessUrlResponse(source_url, proxies);
}

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

}  // namespace chromeos_update_engine
