// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/libcros_proxy.h"

#include "update_engine/dbus_proxies.h"

using org::chromium::LibCrosServiceInterfaceProxy;
using org::chromium::LibCrosServiceInterfaceProxyInterface;
using org::chromium::UpdateEngineLibcrosProxyResolvedInterfaceProxy;
using org::chromium::UpdateEngineLibcrosProxyResolvedInterfaceProxyInterface;

namespace {
const char kLibCrosServiceName[] = "org.chromium.LibCrosService";
}  // namespace

namespace chromeos_update_engine {

LibCrosProxy::LibCrosProxy(
    std::unique_ptr<LibCrosServiceInterfaceProxyInterface>
        service_interface_proxy,
    std::unique_ptr<UpdateEngineLibcrosProxyResolvedInterfaceProxyInterface>
        ue_proxy_resolved_interface)
    : service_interface_proxy_(std::move(service_interface_proxy)),
      ue_proxy_resolved_interface_(std::move(ue_proxy_resolved_interface)) {
}

LibCrosProxy::LibCrosProxy(const scoped_refptr<dbus::Bus>& bus)
    : service_interface_proxy_(
          new LibCrosServiceInterfaceProxy(bus, kLibCrosServiceName)),
      ue_proxy_resolved_interface_(
          new UpdateEngineLibcrosProxyResolvedInterfaceProxy(
              bus,
              kLibCrosServiceName)) {
}

LibCrosServiceInterfaceProxyInterface* LibCrosProxy::service_interface_proxy() {
  return service_interface_proxy_.get();
}

UpdateEngineLibcrosProxyResolvedInterfaceProxyInterface*
LibCrosProxy::ue_proxy_resolved_interface() {
  return ue_proxy_resolved_interface_.get();
}

}  // namespace chromeos_update_engine
