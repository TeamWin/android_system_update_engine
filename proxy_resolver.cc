// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/proxy_resolver.h"

#include <base/bind.h>
#include <base/location.h>

using chromeos::MessageLoop;
using std::deque;
using std::string;

namespace chromeos_update_engine {

const char kNoProxy[] = "direct://";

DirectProxyResolver::~DirectProxyResolver() {
  if (idle_callback_id_ != MessageLoop::kTaskIdNull) {
    // The DirectProxyResolver is instantiated as part of the UpdateAttempter
    // which is also instantiated by default by the FakeSystemState, even when
    // it is not used. We check the manage_shares_id_ before calling the
    // MessageLoop::current() since the unit test using a FakeSystemState may
    // have not define a MessageLoop for the current thread.
    MessageLoop::current()->CancelTask(idle_callback_id_);
    idle_callback_id_ = MessageLoop::kTaskIdNull;
  }
}

bool DirectProxyResolver::GetProxiesForUrl(const string& url,
                                           ProxiesResolvedFn callback,
                                           void* data) {
  idle_callback_id_ = MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
            &DirectProxyResolver::ReturnCallback,
            base::Unretained(this),
            callback,
            data));
  return true;
}

void DirectProxyResolver::ReturnCallback(ProxiesResolvedFn callback,
                                         void* data) {
  idle_callback_id_ = MessageLoop::kTaskIdNull;

  // Initialize proxy pool with as many proxies as indicated (all identical).
  deque<string> proxies(num_proxies_, kNoProxy);

  (*callback)(proxies, data);
}


}  // namespace chromeos_update_engine
