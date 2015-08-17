// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_FAKE_SHILL_PROXY_H_
#define UPDATE_ENGINE_FAKE_SHILL_PROXY_H_

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>

#include "update_engine/dbus_mocks.h"
#include "update_engine/dbus_proxies.h"
#include "update_engine/shill_proxy_interface.h"

namespace chromeos_update_engine {

// This class implements the connection to shill using real DBus calls.
class FakeShillProxy : public ShillProxyInterface {
 public:
  FakeShillProxy();
  ~FakeShillProxy() override = default;

  // ShillProxyInterface overrides.

  // GetManagerProxy returns the subclass ManagerProxyMock so tests can easily
  // use it. Mocks for the return value of GetServiceForPath() can be provided
  // with SetServiceForPath().
  org::chromium::flimflam::ManagerProxyMock* GetManagerProxy() override;
  std::unique_ptr<org::chromium::flimflam::ServiceProxyInterface>
  GetServiceForPath(const std::string& path) override;

  // Sets the service_proxy that will be returned by GetServiceForPath().
  void SetServiceForPath(
      const std::string& path,
      std::unique_ptr<org::chromium::flimflam::ServiceProxyInterface>
          service_proxy);

 private:
  std::unique_ptr<org::chromium::flimflam::ManagerProxyMock>
      manager_proxy_mock_;

  std::map<std::string,
           std::unique_ptr<org::chromium::flimflam::ServiceProxyInterface>>
      service_proxy_mocks_;

  DISALLOW_COPY_AND_ASSIGN(FakeShillProxy);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_FAKE_SHILL_PROXY_H_
