// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_DBUS_INTERFACE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_DBUS_INTERFACE_H__

#include <gmock/gmock.h>

#include "update_engine/dbus_interface.h"

namespace chromeos_update_engine {

class MockDbusGlib : public DbusGlibInterface {
 public:
  MOCK_METHOD4(ProxyNewForName, DBusGProxy*(DBusGConnection *connection,
                                            const char *name,
                                            const char *path,
                                            const char *interface));

  MOCK_METHOD1(ProxyUnref, void(DBusGProxy* proxy));

  MOCK_METHOD2(BusGet, DBusGConnection*(DBusBusType type, GError **error));

  MOCK_METHOD4(ProxyCall_0_1, gboolean(DBusGProxy *proxy,
                                       const char *method,
                                       GError **error,
                                       GHashTable** out1));
  MOCK_METHOD6(ProxyCall_3_0, gboolean(DBusGProxy* proxy,
                                       const char* method,
                                       GError** error,
                                       const char* in1,
                                       const char* in2,
                                       const char* in3));

  MOCK_METHOD1(ConnectionGetConnection, DBusConnection*(DBusGConnection* gbus));

  MOCK_METHOD3(DbusBusAddMatch, void(DBusConnection* connection,
                                     const char* rule,
                                     DBusError* error));

  MOCK_METHOD4(DbusConnectionAddFilter, dbus_bool_t(
      DBusConnection* connection,
      DBusHandleMessageFunction function,
      void* user_data,
      DBusFreeFunction free_data_function));

  MOCK_METHOD3(DbusConnectionRemoveFilter, void(
      DBusConnection* connection,
      DBusHandleMessageFunction function,
      void* user_data));

  MOCK_METHOD3(DbusMessageIsSignal, dbus_bool_t(DBusMessage* message,
                                                const char* interface,
                                                const char* signal_name));

  MOCK_METHOD5(DbusMessageGetArgs_3, dbus_bool_t(DBusMessage* message,
                                                 DBusError* error,
                                                 char** out1,
                                                 char** out2,
                                                 char** out3));
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_DBUS_INTERFACE_H__
