// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_REAL_DBUS_WRAPPER_H_
#define UPDATE_ENGINE_REAL_DBUS_WRAPPER_H_

// A mockable interface for DBus.

#include <base/macros.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "update_engine/dbus_wrapper_interface.h"

namespace chromeos_update_engine {

class RealDBusWrapper : public DBusWrapperInterface {
  virtual DBusGProxy* ProxyNewForName(DBusGConnection* connection,
                                      const char* name,
                                      const char* path,
                                      const char* interface) OVERRIDE {
    return dbus_g_proxy_new_for_name(connection,
                                     name,
                                     path,
                                     interface);
  }

  virtual void ProxyUnref(DBusGProxy* proxy) OVERRIDE {
    g_object_unref(proxy);
  }

  virtual DBusGConnection* BusGet(DBusBusType type, GError** error) OVERRIDE {
    return dbus_g_bus_get(type, error);
  }

  virtual gboolean ProxyCall_0_1(DBusGProxy* proxy,
                                 const char* method,
                                 GError** error,
                                 GHashTable** out1) OVERRIDE {
    return dbus_g_proxy_call(proxy, method, error, G_TYPE_INVALID,
                             dbus_g_type_get_map("GHashTable",
                                                 G_TYPE_STRING,
                                                 G_TYPE_VALUE),
                             out1, G_TYPE_INVALID);
  }

  virtual gboolean ProxyCall_1_0(DBusGProxy* proxy,
                                 const char* method,
                                 GError** error,
                                 gint in1) OVERRIDE {
    return dbus_g_proxy_call(proxy, method, error,
                             G_TYPE_INT, in1,
                             G_TYPE_INVALID, G_TYPE_INVALID);
  }

  virtual gboolean ProxyCall_3_0(DBusGProxy* proxy,
                                 const char* method,
                                 GError** error,
                                 const char* in1,
                                 const char* in2,
                                 const char* in3) OVERRIDE {
    return dbus_g_proxy_call(
        proxy, method, error,
        G_TYPE_STRING, in1, G_TYPE_STRING, in2, G_TYPE_STRING, in3,
        G_TYPE_INVALID, G_TYPE_INVALID);
  }

  virtual void ProxyAddSignal_2(DBusGProxy* proxy,
                                const char* signal_name,
                                GType type1,
                                GType type2) OVERRIDE {
    dbus_g_proxy_add_signal(proxy, signal_name, type1, type2, G_TYPE_INVALID);
  }

  virtual void ProxyConnectSignal(DBusGProxy* proxy,
                                  const char* signal_name,
                                  GCallback handler,
                                  void* data,
                                  GClosureNotify free_data_func) OVERRIDE {
    dbus_g_proxy_connect_signal(proxy, signal_name, handler, data,
                                free_data_func);
  }

  virtual void ProxyDisconnectSignal(DBusGProxy* proxy,
                                     const char* signal_name,
                                     GCallback handler,
                                     void* data) OVERRIDE {
    dbus_g_proxy_disconnect_signal(proxy, signal_name, handler, data);
  }

  virtual DBusConnection* ConnectionGetConnection(
      DBusGConnection* gbus) OVERRIDE {
    return dbus_g_connection_get_connection(gbus);
  }

  virtual void DBusBusAddMatch(DBusConnection* connection,
                               const char* rule,
                               DBusError* error) OVERRIDE {
    dbus_bus_add_match(connection, rule, error);
  }

  virtual dbus_bool_t DBusConnectionAddFilter(
      DBusConnection* connection,
      DBusHandleMessageFunction function,
      void* user_data,
      DBusFreeFunction free_data_function) OVERRIDE {
    return dbus_connection_add_filter(connection,
                                      function,
                                      user_data,
                                      free_data_function);
  }

  virtual void DBusConnectionRemoveFilter(DBusConnection* connection,
                                          DBusHandleMessageFunction function,
                                          void* user_data) OVERRIDE {
    dbus_connection_remove_filter(connection, function, user_data);
  }

  dbus_bool_t DBusMessageIsSignal(DBusMessage* message,
                                  const char* interface,
                                  const char* signal_name) OVERRIDE {
    return dbus_message_is_signal(message, interface, signal_name);
  }

  virtual dbus_bool_t DBusMessageGetArgs_3(DBusMessage* message,
                                           DBusError* error,
                                           char** out1,
                                           char** out2,
                                           char** out3) OVERRIDE {
    return dbus_message_get_args(message, error,
                                 DBUS_TYPE_STRING, out1,
                                 DBUS_TYPE_STRING, out2,
                                 DBUS_TYPE_STRING, out3,
                                 G_TYPE_INVALID);
  }
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_REAL_DBUS_WRAPPER_H_
