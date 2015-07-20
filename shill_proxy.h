// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_SHILL_PROXY_H_
#define UPDATE_ENGINE_SHILL_PROXY_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <dbus/bus.h>

#include "update_engine/dbus_proxies.h"
#include "update_engine/shill_proxy_interface.h"

namespace chromeos_update_engine {

// This class implements the connection to shill using real DBus calls.
class ShillProxy : public ShillProxyInterface {
 public:
  explicit ShillProxy(const scoped_refptr<dbus::Bus>& bus);
  ~ShillProxy() override = default;

  // Initializes the ShillProxy instance creating the manager proxy from the
  // |bus_|.
  bool Init();

  // ShillProxyInterface overrides.
  org::chromium::flimflam::ManagerProxyInterface* GetManagerProxy() override;
  std::unique_ptr<org::chromium::flimflam::ServiceProxyInterface>
  GetServiceForPath(const std::string& path) override;

 private:
  // A reference to the main bus for creating new ServiceProxy instances.
  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<org::chromium::flimflam::ManagerProxyInterface>
      manager_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ShillProxy);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_SHILL_PROXY_H_
