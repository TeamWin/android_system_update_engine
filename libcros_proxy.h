// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_LIBCROS_PROXY_H_
#define UPDATE_ENGINE_LIBCROS_PROXY_H_

#include <memory>

#include <base/macros.h>
#include <dbus/bus.h>

#include "update_engine/dbus_proxies.h"

namespace chromeos_update_engine {

// This class handles the DBus connection with chrome to resolve proxies. This
// is a thin class to just hold the generated proxies (real or mocked ones).
class LibCrosProxy final {
 public:
  explicit LibCrosProxy(const scoped_refptr<dbus::Bus>& bus);
  LibCrosProxy(
      std::unique_ptr<org::chromium::LibCrosServiceInterfaceProxyInterface>
          service_interface_proxy,
      std::unique_ptr<
          org::chromium::
              UpdateEngineLibcrosProxyResolvedInterfaceProxyInterface>
          ue_proxy_resolved_interface);

  ~LibCrosProxy() = default;

  // Getters for the two proxies.
  org::chromium::LibCrosServiceInterfaceProxyInterface*
  service_interface_proxy();
  org::chromium::UpdateEngineLibcrosProxyResolvedInterfaceProxyInterface*
  ue_proxy_resolved_interface();

 private:
  std::unique_ptr<org::chromium::LibCrosServiceInterfaceProxyInterface>
      service_interface_proxy_;
  std::unique_ptr<
      org::chromium::UpdateEngineLibcrosProxyResolvedInterfaceProxyInterface>
      ue_proxy_resolved_interface_;

  DISALLOW_COPY_AND_ASSIGN(LibCrosProxy);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_LIBCROS_PROXY_H_
