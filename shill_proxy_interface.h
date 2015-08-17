// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_SHILL_PROXY_INTERFACE_H_
#define UPDATE_ENGINE_SHILL_PROXY_INTERFACE_H_

#include <memory>
#include <string>

#include <base/macros.h>

#include "update_engine/dbus_proxies.h"

namespace chromeos_update_engine {

// This class handles the DBus connection with shill daemon. The DBus interface
// with shill requires to monitor or request the current service by interacting
// with the org::chromium::flimflam::ManagerProxy and then request or monitor
// properties on the selected org::chromium::flimflam::ServiceProxy. This class
// provides a mockable way to access that.
class ShillProxyInterface {
 public:
  virtual ~ShillProxyInterface() = default;

  // Return the ManagerProxy instance of the shill daemon. The instance is owned
  // by this ShillProxyInterface instance.
  virtual org::chromium::flimflam::ManagerProxyInterface* GetManagerProxy() = 0;

  // Return a ServiceProxy for the given path. The ownership of the returned
  // instance is transferred to the caller.
  virtual std::unique_ptr<org::chromium::flimflam::ServiceProxyInterface>
  GetServiceForPath(const std::string& path) = 0;

 protected:
  ShillProxyInterface() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillProxyInterface);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_SHILL_PROXY_INTERFACE_H_
