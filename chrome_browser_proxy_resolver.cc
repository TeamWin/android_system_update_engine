// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/chrome_browser_proxy_resolver.h"

#include <map>
#include <string>

#include <base/string_util.h>
#include <base/strings/string_tokenizer.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <google/protobuf/stubs/common.h>

#include "update_engine/dbus_constants.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

using base::StringTokenizer;
using google::protobuf::Closure;
using google::protobuf::NewCallback;
using std::deque;
using std::make_pair;
using std::multimap;
using std::pair;
using std::string;

#define LIB_CROS_PROXY_RESOLVE_NAME "ProxyResolved"
#define LIB_CROS_PROXY_RESOLVE_SIGNAL_INTERFACE                 \
  "org.chromium.UpdateEngineLibcrosProxyResolvedInterface"
const char kLibCrosServiceName[] = "org.chromium.LibCrosService";
const char kLibCrosServicePath[] = "/org/chromium/LibCrosService";
const char kLibCrosServiceInterface[] = "org.chromium.LibCrosServiceInterface";
const char kLibCrosServiceResolveNetworkProxyMethodName[] =
    "ResolveNetworkProxy";
const char kLibCrosProxyResolveName[] = LIB_CROS_PROXY_RESOLVE_NAME;
const char kLibCrosProxyResolveSignalInterface[] =
    LIB_CROS_PROXY_RESOLVE_SIGNAL_INTERFACE;
const char kLibCrosProxyResolveSignalFilter[] = "type='signal', "
    "interface='" LIB_CROS_PROXY_RESOLVE_SIGNAL_INTERFACE "', "
    "member='" LIB_CROS_PROXY_RESOLVE_NAME "'";
#undef LIB_CROS_PROXY_RESOLVE_SIGNAL_INTERFACE
#undef LIB_CROS_PROXY_RESOLVE_NAME

namespace {

const int kTimeout = 5;  // seconds

}  // namespace {}

ChromeBrowserProxyResolver::ChromeBrowserProxyResolver(DbusGlibInterface* dbus)
    : dbus_(dbus), proxy_(NULL), timeout_(kTimeout) {}

bool ChromeBrowserProxyResolver::Init() {
  if (proxy_)
    return true;  // Already initialized.

  // Set up signal handler. Code lifted from libcros.
  GError* g_error = NULL;
  DBusGConnection* bus = dbus_->BusGet(DBUS_BUS_SYSTEM, &g_error);
  TEST_AND_RETURN_FALSE(bus);
  DBusConnection* connection = dbus_->ConnectionGetConnection(bus);
  TEST_AND_RETURN_FALSE(connection);

  DBusError dbus_error;
  dbus_error_init(&dbus_error);
  dbus_->DbusBusAddMatch(connection, kLibCrosProxyResolveSignalFilter,
                         &dbus_error);
  TEST_AND_RETURN_FALSE(!dbus_error_is_set(&dbus_error));
  TEST_AND_RETURN_FALSE(dbus_->DbusConnectionAddFilter(
      connection,
      &ChromeBrowserProxyResolver::StaticFilterMessage,
      this,
      NULL));

  proxy_ = dbus_->ProxyNewForName(bus, kLibCrosServiceName, kLibCrosServicePath,
                                  kLibCrosServiceInterface);
  if (!proxy_) {
    dbus_->DbusConnectionRemoveFilter(
        connection,
        &ChromeBrowserProxyResolver::StaticFilterMessage,
        this);
  }
  TEST_AND_RETURN_FALSE(proxy_);  // For the error log
  return true;
}

ChromeBrowserProxyResolver::~ChromeBrowserProxyResolver() {
  // Remove DBus connection filters and Kill proxy object.
  if (proxy_) {
    GError* gerror = NULL;
    DBusGConnection* gbus = dbus_->BusGet(DBUS_BUS_SYSTEM, &gerror);
    if (gbus) {
      DBusConnection* connection = dbus_->ConnectionGetConnection(gbus);
      dbus_->DbusConnectionRemoveFilter(
          connection,
          &ChromeBrowserProxyResolver::StaticFilterMessage,
          this);
    }
    dbus_->ProxyUnref(proxy_);
  }

  // Kill outstanding timers
  for (TimeoutsMap::iterator it = timers_.begin(), e = timers_.end(); it != e;
       ++it) {
    g_source_destroy(it->second);
    it->second = NULL;
  }
}

bool ChromeBrowserProxyResolver::GetProxiesForUrl(const string& url,
                                                  ProxiesResolvedFn callback,
                                                  void* data) {
  GError* error = NULL;
  guint timeout = timeout_;
  if (proxy_) {
    if (!dbus_->ProxyCall_3_0(proxy_,
                              kLibCrosServiceResolveNetworkProxyMethodName,
                              &error,
                              url.c_str(),
                              kLibCrosProxyResolveSignalInterface,
                              kLibCrosProxyResolveName)) {

      if (error) {
        LOG(WARNING) << "dbus_g_proxy_call failed, continuing with no proxy: "
                     << utils::GetAndFreeGError(&error);
      } else {
        LOG(WARNING) << "dbus_g_proxy_call failed with no error string, "
                        "continuing with no proxy.";
      }
      timeout = 0;
    }
  } else {
    LOG(WARNING) << "dbus proxy object missing, continuing with no proxy.";
    timeout = 0;
  }

  callbacks_.insert(make_pair(url, make_pair(callback, data)));
  Closure* closure = NewCallback(this,
                                 &ChromeBrowserProxyResolver::HandleTimeout,
                                 url);
  GSource* timer = g_timeout_source_new_seconds(timeout);
  g_source_set_callback(timer, &utils::GlibRunClosure, closure, NULL);
  g_source_attach(timer, NULL);
  timers_.insert(make_pair(url, timer));
  return true;
}

DBusHandlerResult ChromeBrowserProxyResolver::FilterMessage(
    DBusConnection* connection,
    DBusMessage* message) {
  // Code lifted from libcros.
  if (!dbus_->DbusMessageIsSignal(message,
                                  kLibCrosProxyResolveSignalInterface,
                                  kLibCrosProxyResolveName)) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  // Get args
  char* source_url = NULL;
  char* proxy_list = NULL;
  char* error = NULL;
  DBusError arg_error;
  dbus_error_init(&arg_error);
  if (!dbus_->DbusMessageGetArgs_3(message, &arg_error,
                                   &source_url,
                                   &proxy_list,
                                   &error)) {
    LOG(ERROR) << "Error reading dbus signal.";
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (!source_url || !proxy_list) {
    LOG(ERROR) << "Error getting url, proxy list from dbus signal.";
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  HandleReply(source_url, proxy_list);
  return DBUS_HANDLER_RESULT_HANDLED;
}

bool ChromeBrowserProxyResolver::DeleteUrlState(
    const string& source_url,
    bool delete_timer,
    pair<ProxiesResolvedFn, void*>* callback) {
  {
    CallbacksMap::iterator it = callbacks_.lower_bound(source_url);
    TEST_AND_RETURN_FALSE(it != callbacks_.end());
    TEST_AND_RETURN_FALSE(it->first == source_url);
    if (callback)
      *callback = it->second;
    callbacks_.erase(it);
  }
  {
    TimeoutsMap::iterator it = timers_.lower_bound(source_url);
    TEST_AND_RETURN_FALSE(it != timers_.end());
    TEST_AND_RETURN_FALSE(it->first == source_url);
    if (delete_timer)
      g_source_destroy(it->second);
    timers_.erase(it);
  }
  return true;
}

void ChromeBrowserProxyResolver::HandleReply(const string& source_url,
                                             const string& proxy_list) {
  pair<ProxiesResolvedFn, void*> callback;
  TEST_AND_RETURN(DeleteUrlState(source_url, true, &callback));
  (*callback.first)(ParseProxyString(proxy_list), callback.second);
}

void ChromeBrowserProxyResolver::HandleTimeout(string source_url) {
  LOG(INFO) << "Timeout handler called. Seems Chrome isn't responding.";
  pair<ProxiesResolvedFn, void*> callback;
  TEST_AND_RETURN(DeleteUrlState(source_url, false, &callback));
  deque<string> proxies;
  proxies.push_back(kNoProxy);
  (*callback.first)(proxies, callback.second);
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
    TrimWhitespaceASCII(token, TRIM_ALL, &token);

    // Start by finding the first space (if any).
    std::string::iterator space;
    for (space = token.begin(); space != token.end(); ++space) {
      if (IsAsciiWhitespace(*space)) {
        break;
      }
    }

    string scheme = string(token.begin(), space);
    StringToLowerASCII(&scheme);
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
    TrimWhitespaceASCII(host_and_port, TRIM_ALL, &host_and_port);
    if (scheme != "direct" && host_and_port.empty())
      continue;  // Must supply host/port when non-direct proxy used.
    ret.push_back(scheme + "://" + host_and_port);
  }
  if (ret.empty() || *ret.rbegin() != kNoProxy)
    ret.push_back(kNoProxy);
  return ret;
}

}  // namespace chromeos_update_engine
