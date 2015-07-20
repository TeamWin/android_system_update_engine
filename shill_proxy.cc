// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/shill_proxy.h"

#include <chromeos/dbus/service_constants.h>

#include "update_engine/dbus_proxies.h"

using org::chromium::flimflam::ManagerProxy;
using org::chromium::flimflam::ManagerProxyInterface;
using org::chromium::flimflam::ServiceProxy;
using org::chromium::flimflam::ServiceProxyInterface;

namespace chromeos_update_engine {

ShillProxy::ShillProxy(const scoped_refptr<dbus::Bus>& bus) : bus_(bus) {}

bool ShillProxy::Init() {
  manager_proxy_.reset(
      new ManagerProxy(bus_,
                       shill::kFlimflamServiceName,
                       dbus::ObjectPath(shill::kFlimflamServicePath)));
  return true;
}

ManagerProxyInterface* ShillProxy::GetManagerProxy() {
  return manager_proxy_.get();
}

std::unique_ptr<ServiceProxyInterface> ShillProxy::GetServiceForPath(
    const std::string& path) {
  DCHECK(bus_.get());
  return std::unique_ptr<ServiceProxyInterface>(new ServiceProxy(
      bus_, shill::kFlimflamServiceName, dbus::ObjectPath(path)));
}

}  // namespace chromeos_update_engine
