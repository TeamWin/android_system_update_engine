// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/fake_shill_proxy.h"

#include "update_engine/dbus_proxies.h"

using org::chromium::flimflam::ManagerProxyMock;
using org::chromium::flimflam::ServiceProxyInterface;

namespace chromeos_update_engine {

FakeShillProxy::FakeShillProxy()
    : manager_proxy_mock_(new ManagerProxyMock()) {}

ManagerProxyMock* FakeShillProxy::GetManagerProxy() {
  return manager_proxy_mock_.get();
}

std::unique_ptr<ServiceProxyInterface> FakeShillProxy::GetServiceForPath(
    const std::string& path) {
  auto it = service_proxy_mocks_.find(path);
  CHECK(it != service_proxy_mocks_.end()) << "No ServiceProxyMock set for "
                                          << path;
  std::unique_ptr<ServiceProxyInterface> result = std::move(it->second);
  service_proxy_mocks_.erase(it);
  return result;
}

void FakeShillProxy::SetServiceForPath(
    const std::string& path,
    std::unique_ptr<ServiceProxyInterface> service_proxy) {
  service_proxy_mocks_[path] = std::move(service_proxy);
}

}  // namespace chromeos_update_engine
