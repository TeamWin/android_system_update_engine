// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_INTERFACE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_INTERFACE_H__

// A mockable interface for DBus.

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#ifndef DBUS_TYPE_G_OBJECT_PATH_ARRAY
#define DBUS_TYPE_G_OBJECT_PATH_ARRAY \
  (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH))
#endif

#ifndef DBUS_TYPE_G_STRING_ARRAY
#define DBUS_TYPE_G_STRING_ARRAY \
  (dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRING))
#endif

namespace chromeos_update_engine {

class DbusGlibInterface {
 public:
  // Wraps dbus_g_proxy_new_for_name().
  virtual DBusGProxy* ProxyNewForName(DBusGConnection* connection,
                                      const char* name,
                                      const char* path,
                                      const char* interface) = 0;

  // Wraps g_object_unref().
  virtual void ProxyUnref(DBusGProxy* proxy) = 0;

  // Wraps dbus_g_bus_get().
  virtual DBusGConnection* BusGet(DBusBusType type, GError** error) = 0;

  // Wraps dbus_g_proxy_call(). Since this is a variadic function without a
  // va_list equivalent, we have to list specific wrappers depending on the
  // number of input and output arguments, based on the required usage. Note,
  // however, that we do rely on automatic signature overriding to facilitate
  // different types of input/output arguments.
  virtual gboolean ProxyCall_0_1(DBusGProxy* proxy,
                                 const char* method,
                                 GError** error,
                                 GHashTable** out1) = 0;

  virtual gboolean ProxyCall_3_0(DBusGProxy* proxy,
                                 const char* method,
                                 GError** error,
                                 const char* in1,
                                 const char* in2,
                                 const char* in3) = 0;

  // Wraps dbus_g_connection_get_connection().
  virtual DBusConnection* ConnectionGetConnection(DBusGConnection* gbus) = 0;

  // Wraps dbus_bus_add_match().
  virtual void DbusBusAddMatch(DBusConnection* connection,
                               const char* rule,
                               DBusError* error) = 0;

  // Wraps dbus_connection_add_filter().
  virtual dbus_bool_t DbusConnectionAddFilter(
      DBusConnection* connection,
      DBusHandleMessageFunction function,
      void* user_data,
      DBusFreeFunction free_data_function) = 0;

  // Wraps dbus_connection_remove_filter().
  virtual void DbusConnectionRemoveFilter(DBusConnection* connection,
                                          DBusHandleMessageFunction function,
                                          void* user_data) = 0;

  // Wraps dbus_message_is_signal().
  virtual dbus_bool_t DbusMessageIsSignal(DBusMessage* message,
                                          const char* interface,
                                          const char* signal_name) = 0;

  // Wraps dbus_message_get_args(). Deploys the same approach for handling
  // variadic arguments as ProxyCall above.
  virtual dbus_bool_t DbusMessageGetArgs_3(DBusMessage* message,
                                           DBusError* error,
                                           char** out1,
                                           char** out2,
                                           char** out3) = 0;
};


class ConcreteDbusGlib : public DbusGlibInterface {
  virtual DBusGProxy* ProxyNewForName(DBusGConnection* connection,
                                      const char* name,
                                      const char* path,
                                      const char* interface) {
    return dbus_g_proxy_new_for_name(connection,
                                     name,
                                     path,
                                     interface);
  }

  virtual void ProxyUnref(DBusGProxy* proxy) {
    g_object_unref(proxy);
  }

  virtual DBusGConnection* BusGet(DBusBusType type, GError** error) {
    return dbus_g_bus_get(type, error);
  }

  virtual gboolean ProxyCall_0_1(DBusGProxy* proxy,
                                 const char* method,
                                 GError** error,
                                 GHashTable** out1) {
    return dbus_g_proxy_call(proxy, method, error, G_TYPE_INVALID,
                             dbus_g_type_get_map("GHashTable",
                                                 G_TYPE_STRING,
                                                 G_TYPE_VALUE),
                             out1, G_TYPE_INVALID);
  }

  virtual gboolean ProxyCall_3_0(DBusGProxy* proxy,
                                 const char* method,
                                 GError** error,
                                 const char* in1,
                                 const char* in2,
                                 const char* in3) {
    return dbus_g_proxy_call(
        proxy, method, error,
        G_TYPE_STRING, in1, G_TYPE_STRING, in2, G_TYPE_STRING, in3,
        G_TYPE_INVALID, G_TYPE_INVALID);
  }

  virtual DBusConnection* ConnectionGetConnection(DBusGConnection* gbus) {
    return dbus_g_connection_get_connection(gbus);
  }

  virtual void DbusBusAddMatch(DBusConnection* connection,
                               const char* rule,
                               DBusError* error) {
    dbus_bus_add_match(connection, rule, error);
  }

  virtual dbus_bool_t DbusConnectionAddFilter(
      DBusConnection* connection,
      DBusHandleMessageFunction function,
      void* user_data,
      DBusFreeFunction free_data_function) {
    return dbus_connection_add_filter(connection,
                                      function,
                                      user_data,
                                      free_data_function);
  }

  virtual void DbusConnectionRemoveFilter(DBusConnection* connection,
                                          DBusHandleMessageFunction function,
                                          void* user_data) {
    dbus_connection_remove_filter(connection, function, user_data);
  }

  dbus_bool_t DbusMessageIsSignal(DBusMessage* message,
                                  const char* interface,
                                  const char* signal_name) {
    return dbus_message_is_signal(message, interface, signal_name);
  }

  virtual dbus_bool_t DbusMessageGetArgs_3(DBusMessage* message,
                                           DBusError* error,
                                           char** out1,
                                           char** out2,
                                           char** out3) {
    return dbus_message_get_args(message, error,
                                 DBUS_TYPE_STRING, out1,
                                 DBUS_TYPE_STRING, out2,
                                 DBUS_TYPE_STRING, out3,
                                 G_TYPE_INVALID);
  }
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_INTERFACE_H__
