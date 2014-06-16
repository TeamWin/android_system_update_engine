// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_WRAPPER_INTERFACE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_WRAPPER_INTERFACE_H_

// A mockable interface for DBus.

#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>

#ifndef DBUS_TYPE_G_OBJECT_PATH_ARRAY
#define DBUS_TYPE_G_OBJECT_PATH_ARRAY \
  (dbus_g_type_get_collection("GPtrArray", DBUS_TYPE_G_OBJECT_PATH))
#endif

#ifndef DBUS_TYPE_G_STRING_ARRAY
#define DBUS_TYPE_G_STRING_ARRAY \
  (dbus_g_type_get_collection("GPtrArray", G_TYPE_STRING))
#endif

namespace chromeos_update_engine {

class DBusWrapperInterface {
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

  // Wrappers for dbus_g_proxy_add_signal() (variadic).
  virtual void ProxyAddSignal_2(DBusGProxy* proxy,
                                const char* signal_name,
                                GType type1,
                                GType type2) = 0;

  // Wrapper for dbus_g_proxy_{connect,disconnect}_signal().
  virtual void ProxyConnectSignal(DBusGProxy* proxy,
                                  const char* signal_name,
                                  GCallback handler,
                                  void* data,
                                  GClosureNotify free_data_func) = 0;
  virtual void ProxyDisconnectSignal(DBusGProxy* proxy,
                                     const char* signal_name,
                                     GCallback handler,
                                     void* data) = 0;

  // Wraps dbus_g_connection_get_connection().
  virtual DBusConnection* ConnectionGetConnection(DBusGConnection* gbus) = 0;

  // Wraps dbus_bus_add_match().
  virtual void DBusBusAddMatch(DBusConnection* connection,
                               const char* rule,
                               DBusError* error) = 0;

  // Wraps dbus_connection_add_filter().
  virtual dbus_bool_t DBusConnectionAddFilter(
      DBusConnection* connection,
      DBusHandleMessageFunction function,
      void* user_data,
      DBusFreeFunction free_data_function) = 0;

  // Wraps dbus_connection_remove_filter().
  virtual void DBusConnectionRemoveFilter(DBusConnection* connection,
                                          DBusHandleMessageFunction function,
                                          void* user_data) = 0;

  // Wraps dbus_message_is_signal().
  virtual dbus_bool_t DBusMessageIsSignal(DBusMessage* message,
                                          const char* interface,
                                          const char* signal_name) = 0;

  // Wraps dbus_message_get_args(). Deploys the same approach for handling
  // variadic arguments as ProxyCall above.
  virtual dbus_bool_t DBusMessageGetArgs_3(DBusMessage* message,
                                           DBusError* error,
                                           char** out1,
                                           char** out2,
                                           char** out3) = 0;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_WRAPPER_INTERFACE_H_
