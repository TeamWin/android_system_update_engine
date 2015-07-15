// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_MOCK_DBUS_WRAPPER_H_
#define UPDATE_ENGINE_MOCK_DBUS_WRAPPER_H_

#include <gmock/gmock.h>

#include "update_engine/dbus_wrapper_interface.h"

namespace chromeos_update_engine {

class MockDBusWrapper : public DBusWrapperInterface {
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
  MOCK_METHOD4(ProxyCall_0_1, gboolean(DBusGProxy *proxy,
                                       const char *method,
                                       GError **error,
                                       gint* out1));
  MOCK_METHOD4(ProxyCall_1_0, gboolean(DBusGProxy *proxy,
                                       const char *method,
                                       GError **error,
                                       gint in1));
  MOCK_METHOD6(ProxyCall_3_0, gboolean(DBusGProxy* proxy,
                                       const char* method,
                                       GError** error,
                                       const char* in1,
                                       const char* in2,
                                       const char* in3));

  MOCK_METHOD3(ProxyAddSignal_1, void(DBusGProxy* proxy,
                                      const char* signal_name,
                                      GType type1));

  MOCK_METHOD4(ProxyAddSignal_2, void(DBusGProxy* proxy,
                                      const char* signal_name,
                                      GType type1,
                                      GType type2));

  MOCK_METHOD5(ProxyConnectSignal, void(DBusGProxy* proxy,
                                        const char* signal_name,
                                        GCallback handler,
                                        void* data,
                                        GClosureNotify free_data_func));

  MOCK_METHOD4(ProxyDisconnectSignal, void(DBusGProxy* proxy,
                                           const char* signal_name,
                                           GCallback handler,
                                           void* data));

  MOCK_METHOD1(ConnectionGetConnection, DBusConnection*(DBusGConnection* gbus));

  MOCK_METHOD3(DBusBusAddMatch, void(DBusConnection* connection,
                                     const char* rule,
                                     DBusError* error));

  MOCK_METHOD4(DBusConnectionAddFilter, dbus_bool_t(
      DBusConnection* connection,
      DBusHandleMessageFunction function,
      void* user_data,
      DBusFreeFunction free_data_function));

  MOCK_METHOD3(DBusConnectionRemoveFilter, void(
      DBusConnection* connection,
      DBusHandleMessageFunction function,
      void* user_data));

  MOCK_METHOD3(DBusMessageIsSignal, dbus_bool_t(DBusMessage* message,
                                                const char* interface,
                                                const char* signal_name));

  MOCK_METHOD5(DBusMessageGetArgs_3, dbus_bool_t(DBusMessage* message,
                                                 DBusError* error,
                                                 char** out1,
                                                 char** out2,
                                                 char** out3));
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_MOCK_DBUS_WRAPPER_H_
