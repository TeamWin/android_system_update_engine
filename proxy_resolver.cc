// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/proxy_resolver.h"

#include <base/bind.h>

using std::deque;
using std::string;

namespace chromeos_update_engine {

const char kNoProxy[] = "direct://";

DirectProxyResolver::~DirectProxyResolver() {
  if (idle_callback_id_) {
    g_source_remove(idle_callback_id_);
    idle_callback_id_ = 0;
  }
}

bool DirectProxyResolver::GetProxiesForUrl(const string& url,
                                           ProxiesResolvedFn callback,
                                           void* data) {
  base::Closure* closure = new base::Closure(base::Bind(
      &DirectProxyResolver::ReturnCallback,
      base::Unretained(this),
      callback,
      data));
  idle_callback_id_ = g_idle_add_full(
      G_PRIORITY_DEFAULT,
      utils::GlibRunClosure,
      closure,
      utils::GlibDestroyClosure);
  return true;
}

void DirectProxyResolver::ReturnCallback(ProxiesResolvedFn callback,
                                         void* data) {
  idle_callback_id_ = 0;

  // Initialize proxy pool with as many proxies as indicated (all identical).
  deque<string> proxies(num_proxies_, kNoProxy);

  (*callback)(proxies, data);
}


}  // namespace chromeos_update_engine
