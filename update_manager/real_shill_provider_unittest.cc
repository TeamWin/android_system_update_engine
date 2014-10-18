// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <glib.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/mock_dbus_wrapper.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_manager/real_shill_provider.h"
#include "update_engine/update_manager/umtest_utils.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::FakeClock;
using chromeos_update_engine::GValueFree;
using chromeos_update_engine::GValueNewString;
using chromeos_update_engine::MockDBusWrapper;
using std::pair;
using std::unique_ptr;
using testing::Eq;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::StrEq;
using testing::StrictMock;
using testing::_;

namespace {

// Fake dbus-glib objects. These should be different values, to ease diagnosis
// of errors.
DBusGConnection* const kFakeConnection = reinterpret_cast<DBusGConnection*>(1);
DBusGProxy* const kFakeManagerProxy = reinterpret_cast<DBusGProxy*>(2);
DBusGProxy* const kFakeEthernetServiceProxy = reinterpret_cast<DBusGProxy*>(3);
DBusGProxy* const kFakeWifiServiceProxy = reinterpret_cast<DBusGProxy*>(4);
DBusGProxy* const kFakeWimaxServiceProxy = reinterpret_cast<DBusGProxy*>(5);
DBusGProxy* const kFakeBluetoothServiceProxy = reinterpret_cast<DBusGProxy*>(6);
DBusGProxy* const kFakeCellularServiceProxy = reinterpret_cast<DBusGProxy*>(7);
DBusGProxy* const kFakeVpnServiceProxy = reinterpret_cast<DBusGProxy*>(8);
DBusGProxy* const kFakeUnknownServiceProxy = reinterpret_cast<DBusGProxy*>(9);

// Fake service paths.
const char* const kFakeEthernetServicePath = "/fake-ethernet-service";
const char* const kFakeWifiServicePath = "/fake-wifi-service";
const char* const kFakeWimaxServicePath = "/fake-wimax-service";
const char* const kFakeBluetoothServicePath = "/fake-bluetooth-service";
const char* const kFakeCellularServicePath = "/fake-cellular-service";
const char* const kFakeVpnServicePath = "/fake-vpn-service";
const char* const kFakeUnknownServicePath = "/fake-unknown-service";

}  // namespace

namespace chromeos_update_manager {

class UmRealShillProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // By default, initialize the provider so that it gets an initial connection
    // status from shill. This simulates the common case where shill is
    // available and responding during RealShillProvider initialization.
    Init(true);
  }

  virtual void TearDown() {
    Shutdown();
  }

  // Initialize the RealShillProvider under test. If |do_init_conn_status| is
  // true, configure mocks to respond to the initial connection status check
  // with shill. Otherwise, the initial check will fail.
  void Init(bool do_init_conn_status) {
    // Properly shutdown a previously initialized provider.
    if (provider_.get())
      Shutdown();

    provider_.reset(new RealShillProvider(&mock_dbus_, &fake_clock_));
    ASSERT_NE(nullptr, provider_.get());
    fake_clock_.SetWallclockTime(InitTime());

    // A DBus connection should only be obtained once.
    EXPECT_CALL(mock_dbus_, BusGet(_, _)).WillOnce(
        Return(kFakeConnection));

    // A manager proxy should only be obtained once.
    EXPECT_CALL(mock_dbus_, ProxyNewForName(
            kFakeConnection, StrEq(shill::kFlimflamServiceName),
            StrEq(shill::kFlimflamServicePath),
            StrEq(shill::kFlimflamManagerInterface)))
        .WillOnce(Return(kFakeManagerProxy));

    // The PropertyChanged signal should be subscribed to.
    EXPECT_CALL(mock_dbus_, ProxyAddSignal_2(
            kFakeManagerProxy, StrEq(shill::kMonitorPropertyChanged),
            G_TYPE_STRING, G_TYPE_VALUE))
        .WillOnce(Return());
    EXPECT_CALL(mock_dbus_, ProxyConnectSignal(
            kFakeManagerProxy, StrEq(shill::kMonitorPropertyChanged),
            _, _, _))
        .WillOnce(
            DoAll(SaveArg<2>(reinterpret_cast<void (**)()>(&signal_handler_)),
                  SaveArg<3>(&signal_data_),
                  Return()));

    // Mock a response to an initial connection check (optional).
    GHashTable* manager_properties = nullptr;
    if (do_init_conn_status) {
      pair<const char*, const char*> manager_pairs[] = {
        {shill::kDefaultServiceProperty, "/"},
      };
      manager_properties = SetupGetPropertiesOkay(
          kFakeManagerProxy, arraysize(manager_pairs), manager_pairs);
    } else {
      SetupGetPropertiesFail(kFakeManagerProxy);
    }

    // Check that provider initializes correctly.
    ASSERT_TRUE(provider_->Init());

    // All mocked calls should have been exercised by now.
    Mock::VerifyAndClear(&mock_dbus_);

    // Release properties hash table (if provided).
    if (manager_properties)
      g_hash_table_unref(manager_properties);
  }

  // Deletes the RealShillProvider under test.
  void Shutdown() {
    // Make sure that DBus resources get freed.
    EXPECT_CALL(mock_dbus_, ProxyDisconnectSignal(
            kFakeManagerProxy, StrEq(shill::kMonitorPropertyChanged),
            Eq(reinterpret_cast<void (*)()>(signal_handler_)),
            Eq(signal_data_)))
        .WillOnce(Return());
    EXPECT_CALL(mock_dbus_, ProxyUnref(kFakeManagerProxy)).WillOnce(Return());
    provider_.reset();

    // All mocked calls should have been exercised by now.
    Mock::VerifyAndClear(&mock_dbus_);
  }

  // These methods generate fixed timestamps for use in faking the current time.
  Time InitTime() {
    Time::Exploded now_exp;
    now_exp.year = 2014;
    now_exp.month = 3;
    now_exp.day_of_week = 2;
    now_exp.day_of_month = 18;
    now_exp.hour = 8;
    now_exp.minute = 5;
    now_exp.second = 33;
    now_exp.millisecond = 675;
    return Time::FromLocalExploded(now_exp);
  }

  Time ConnChangedTime() {
    return InitTime() + TimeDelta::FromSeconds(10);
  }

  // Sets up a successful mock "GetProperties" call on |proxy|, writing a hash
  // table containing |num_entries| entries formed by key/value pairs from
  // |key_val_pairs| and returning true. Keys and values are plain C strings
  // (const char*). The proxy call is expected to be made exactly once. Returns
  // a pointer to a newly allocated hash table, which should be unreffed with
  // g_hash_table_unref() when done.
  GHashTable* SetupGetPropertiesOkay(
      DBusGProxy* proxy, size_t num_entries,
      pair<const char*, const char*>* key_val_pairs) {
    // Allocate and populate the hash table.
    GHashTable* properties = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                   free, GValueFree);
    for (size_t i = 0; i < num_entries; i++) {
      g_hash_table_insert(properties, strdup(key_val_pairs[i].first),
                          GValueNewString(key_val_pairs[i].second));
    }

    // Set mock expectations.
    EXPECT_CALL(mock_dbus_,
                ProxyCall_0_1(proxy, StrEq(shill::kGetPropertiesFunction),
                              _, _))
        .WillOnce(DoAll(SetArgPointee<3>(g_hash_table_ref(properties)),
                        Return(true)));

    return properties;
  }

  // Sets up a failing mock "GetProperties" call on |proxy|, returning false.
  // The proxy call is expected to be made exactly once.
  void SetupGetPropertiesFail(DBusGProxy* proxy) {
    EXPECT_CALL(mock_dbus_,
                ProxyCall_0_1(proxy, StrEq(shill::kGetPropertiesFunction),
                              _, _))
      .WillOnce(Return(false));
  }

  // Sends a signal informing the provider about a default connection
  // |service_path|. Returns the fake connection change time.
  Time SendDefaultServiceSignal(const char* service_path) {
    auto default_service_gval = GValueNewString(service_path);
    const Time conn_change_time = ConnChangedTime();
    fake_clock_.SetWallclockTime(conn_change_time);
    signal_handler_(kFakeManagerProxy, shill::kDefaultServiceProperty,
                    default_service_gval, signal_data_);
    fake_clock_.SetWallclockTime(conn_change_time + TimeDelta::FromSeconds(5));
    GValueFree(default_service_gval);
    return conn_change_time;
  }

  // Sets up expectations for detection of a connection |service_path| with type
  // |shill_type_str| and tethering mode |shill_tethering_str|. Ensures that the
  // new connection status and change time are properly detected by the
  // provider. Writes the fake connection change time to |conn_change_time_p|,
  // if provided.
  void SetupConnectionAndAttrs(const char* service_path,
                               DBusGProxy* service_proxy,
                               const char* shill_type_str,
                               const char* shill_tethering_str,
                               Time* conn_change_time_p) {
    // Mock logic for querying the default service attributes.
    EXPECT_CALL(mock_dbus_,
                ProxyNewForName(
                    kFakeConnection, StrEq(shill::kFlimflamServiceName),
                    StrEq(service_path),
                    StrEq(shill::kFlimflamServiceInterface)))
        .WillOnce(Return(service_proxy));
    EXPECT_CALL(mock_dbus_, ProxyUnref(service_proxy)).WillOnce(Return());
    pair<const char*, const char*> service_pairs[] = {
      {shill::kTypeProperty, shill_type_str},
      {shill::kTetheringProperty, shill_tethering_str},
    };
    auto service_properties = SetupGetPropertiesOkay(
        service_proxy, arraysize(service_pairs), service_pairs);

    // Send a signal about a new default service.
    auto conn_change_time = SendDefaultServiceSignal(service_path);

    // Release the service properties hash tables.
    g_hash_table_unref(service_properties);

    // Query the connection status, ensure last change time reported correctly.
    UmTestUtils::ExpectVariableHasValue(true, provider_->var_is_connected());
    UmTestUtils::ExpectVariableHasValue(conn_change_time,
                                        provider_->var_conn_last_changed());

    // Write the connection change time to the output argument.
    if (conn_change_time_p)
      *conn_change_time_p = conn_change_time;
  }

  // Sets up a connection and tests that its type is being properly detected by
  // the provider.
  void SetupConnectionAndTestType(const char* service_path,
                                  DBusGProxy* service_proxy,
                                  const char* shill_type_str,
                                  ConnectionType expected_conn_type) {
    // Set up and test the connection, record the change time.
    Time conn_change_time;
    SetupConnectionAndAttrs(service_path, service_proxy, shill_type_str,
                            shill::kTetheringNotDetectedState,
                            &conn_change_time);

    // Query the connection type, ensure last change time did not change.
    UmTestUtils::ExpectVariableHasValue(expected_conn_type,
                                        provider_->var_conn_type());
    UmTestUtils::ExpectVariableHasValue(conn_change_time,
                                        provider_->var_conn_last_changed());
  }

  // Sets up a connection and tests that its tethering mode is being properly
  // detected by the provider.
  void SetupConnectionAndTestTethering(
      const char* service_path, DBusGProxy* service_proxy,
      const char* shill_tethering_str,
      ConnectionTethering expected_conn_tethering) {
    // Set up and test the connection, record the change time.
    Time conn_change_time;
    SetupConnectionAndAttrs(service_path, service_proxy, shill::kTypeEthernet,
                            shill_tethering_str, &conn_change_time);

    // Query the connection tethering, ensure last change time did not change.
    UmTestUtils::ExpectVariableHasValue(expected_conn_tethering,
                                        provider_->var_conn_tethering());
    UmTestUtils::ExpectVariableHasValue(conn_change_time,
                                        provider_->var_conn_last_changed());
  }

  StrictMock<MockDBusWrapper> mock_dbus_;
  FakeClock fake_clock_;
  unique_ptr<RealShillProvider> provider_;
  void (*signal_handler_)(DBusGProxy*, const char*, GValue*, void*);
  void* signal_data_;
};

// Query the connection status, type and time last changed, as they were set
// during initialization (no signals).
TEST_F(UmRealShillProviderTest, ReadBaseValues) {
  // Query the provider variables.
  UmTestUtils::ExpectVariableHasValue(false, provider_->var_is_connected());
  UmTestUtils::ExpectVariableNotSet(provider_->var_conn_type());
  UmTestUtils::ExpectVariableHasValue(InitTime(),
                                      provider_->var_conn_last_changed());
}

// Test that Ethernet connection is identified correctly.
TEST_F(UmRealShillProviderTest, ReadConnTypeEthernet) {
  SetupConnectionAndTestType(kFakeEthernetServicePath,
                             kFakeEthernetServiceProxy,
                             shill::kTypeEthernet,
                             ConnectionType::kEthernet);
}

// Test that Wifi connection is identified correctly.
TEST_F(UmRealShillProviderTest, ReadConnTypeWifi) {
  SetupConnectionAndTestType(kFakeWifiServicePath,
                             kFakeWifiServiceProxy,
                             shill::kTypeWifi,
                             ConnectionType::kWifi);
}

// Test that Wimax connection is identified correctly.
TEST_F(UmRealShillProviderTest, ReadConnTypeWimax) {
  SetupConnectionAndTestType(kFakeWimaxServicePath,
                             kFakeWimaxServiceProxy,
                             shill::kTypeWimax,
                             ConnectionType::kWimax);
}

// Test that Bluetooth connection is identified correctly.
TEST_F(UmRealShillProviderTest, ReadConnTypeBluetooth) {
  SetupConnectionAndTestType(kFakeBluetoothServicePath,
                             kFakeBluetoothServiceProxy,
                             shill::kTypeBluetooth,
                             ConnectionType::kBluetooth);
}

// Test that Cellular connection is identified correctly.
TEST_F(UmRealShillProviderTest, ReadConnTypeCellular) {
  SetupConnectionAndTestType(kFakeCellularServicePath,
                             kFakeCellularServiceProxy,
                             shill::kTypeCellular,
                             ConnectionType::kCellular);
}

// Test that an unknown connection is identified as such.
TEST_F(UmRealShillProviderTest, ReadConnTypeUnknown) {
  SetupConnectionAndTestType(kFakeUnknownServicePath,
                             kFakeUnknownServiceProxy,
                             "FooConnectionType",
                             ConnectionType::kUnknown);
}

// Tests that VPN connection is identified correctly.
TEST_F(UmRealShillProviderTest, ReadConnTypeVpn) {
  // Mock logic for returning a default service path and its type.
  EXPECT_CALL(mock_dbus_, ProxyNewForName(
          kFakeConnection, StrEq(shill::kFlimflamServiceName),
          StrEq(kFakeVpnServicePath), StrEq(shill::kFlimflamServiceInterface)))
      .WillOnce(Return(kFakeVpnServiceProxy));
  EXPECT_CALL(mock_dbus_, ProxyUnref(kFakeVpnServiceProxy)).WillOnce(Return());
  pair<const char*, const char*> service_pairs[] = {
    {shill::kTypeProperty, shill::kTypeVPN},
    {shill::kPhysicalTechnologyProperty, shill::kTypeWifi},
  };
  auto service_properties = SetupGetPropertiesOkay(kFakeVpnServiceProxy,
                                                   arraysize(service_pairs),
                                                   service_pairs);

  // Send a signal about a new default service.
  Time conn_change_time = SendDefaultServiceSignal(kFakeVpnServicePath);

  // Query the connection type, ensure last change time reported correctly.
  UmTestUtils::ExpectVariableHasValue(ConnectionType::kWifi,
                                      provider_->var_conn_type());
  UmTestUtils::ExpectVariableHasValue(conn_change_time,
                                      provider_->var_conn_last_changed());

  // Release properties hash tables.
  g_hash_table_unref(service_properties);
}

// Ensure that the connection type is properly cached in the provider through
// subsequent variable readings.
TEST_F(UmRealShillProviderTest, ConnTypeCacheUsed) {
  SetupConnectionAndTestType(kFakeEthernetServicePath,
                             kFakeEthernetServiceProxy,
                             shill::kTypeEthernet,
                             ConnectionType::kEthernet);

  UmTestUtils::ExpectVariableHasValue(ConnectionType::kEthernet,
                                      provider_->var_conn_type());
}

// Ensure that the cached connection type remains valid even when a default
// connection signal occurs but the connection is not changed.
TEST_F(UmRealShillProviderTest, ConnTypeCacheRemainsValid) {
  SetupConnectionAndTestType(kFakeEthernetServicePath,
                             kFakeEthernetServiceProxy,
                             shill::kTypeEthernet,
                             ConnectionType::kEthernet);

  SendDefaultServiceSignal(kFakeEthernetServicePath);

  UmTestUtils::ExpectVariableHasValue(ConnectionType::kEthernet,
                                      provider_->var_conn_type());
}

// Ensure that the cached connection type is invalidated and re-read when the
// default connection changes.
TEST_F(UmRealShillProviderTest, ConnTypeCacheInvalidated) {
  SetupConnectionAndTestType(kFakeEthernetServicePath,
                             kFakeEthernetServiceProxy,
                             shill::kTypeEthernet,
                             ConnectionType::kEthernet);

  SetupConnectionAndTestType(kFakeWifiServicePath,
                             kFakeWifiServiceProxy,
                             shill::kTypeWifi,
                             ConnectionType::kWifi);
}

// Test that a non-tethering mode is identified correctly.
TEST_F(UmRealShillProviderTest, ReadConnTetheringNotDetected) {
  SetupConnectionAndTestTethering(kFakeWifiServicePath,
                                  kFakeWifiServiceProxy,
                                  shill::kTetheringNotDetectedState,
                                  ConnectionTethering::kNotDetected);
}

// Test that a suspected tethering mode is identified correctly.
TEST_F(UmRealShillProviderTest, ReadConnTetheringSuspected) {
  SetupConnectionAndTestTethering(kFakeWifiServicePath,
                                  kFakeWifiServiceProxy,
                                  shill::kTetheringSuspectedState,
                                  ConnectionTethering::kSuspected);
}

// Test that a confirmed tethering mode is identified correctly.
TEST_F(UmRealShillProviderTest, ReadConnTetheringConfirmed) {
  SetupConnectionAndTestTethering(kFakeWifiServicePath,
                                  kFakeWifiServiceProxy,
                                  shill::kTetheringConfirmedState,
                                  ConnectionTethering::kConfirmed);
}

// Test that an unknown tethering mode is identified as such.
TEST_F(UmRealShillProviderTest, ReadConnTetheringUnknown) {
  SetupConnectionAndTestTethering(kFakeWifiServicePath,
                                  kFakeWifiServiceProxy,
                                  "FooConnTethering",
                                  ConnectionTethering::kUnknown);
}

// Ensure that the connection tethering mode is properly cached in the provider.
TEST_F(UmRealShillProviderTest, ConnTetheringCacheUsed) {
  SetupConnectionAndTestTethering(kFakeEthernetServicePath,
                                  kFakeEthernetServiceProxy,
                                  shill::kTetheringNotDetectedState,
                                  ConnectionTethering::kNotDetected);

  UmTestUtils::ExpectVariableHasValue(ConnectionTethering::kNotDetected,
                                      provider_->var_conn_tethering());
}

// Ensure that the cached connection tethering mode remains valid even when a
// default connection signal occurs but the connection is not changed.
TEST_F(UmRealShillProviderTest, ConnTetheringCacheRemainsValid) {
  SetupConnectionAndTestTethering(kFakeEthernetServicePath,
                                  kFakeEthernetServiceProxy,
                                  shill::kTetheringNotDetectedState,
                                  ConnectionTethering::kNotDetected);

  SendDefaultServiceSignal(kFakeEthernetServicePath);

  UmTestUtils::ExpectVariableHasValue(ConnectionTethering::kNotDetected,
                                      provider_->var_conn_tethering());
}

// Ensure that the cached connection tethering mode is invalidated and re-read
// when the default connection changes.
TEST_F(UmRealShillProviderTest, ConnTetheringCacheInvalidated) {
  SetupConnectionAndTestTethering(kFakeEthernetServicePath,
                                  kFakeEthernetServiceProxy,
                                  shill::kTetheringNotDetectedState,
                                  ConnectionTethering::kNotDetected);

  SetupConnectionAndTestTethering(kFakeWifiServicePath,
                                  kFakeWifiServiceProxy,
                                  shill::kTetheringConfirmedState,
                                  ConnectionTethering::kConfirmed);
}

// Fake two DBus signals prompting a default connection change, but otherwise
// give the same service path. Check connection status and the time it was last
// changed, making sure that it is the time when the first signal was sent (and
// not the second).
TEST_F(UmRealShillProviderTest, ReadLastChangedTimeTwoSignals) {
  // Send a default service signal twice, advancing the clock in between.
  Time conn_change_time;
  SetupConnectionAndAttrs(kFakeEthernetServicePath, kFakeEthernetServiceProxy,
                          shill::kTypeEthernet,
                          shill::kTetheringNotDetectedState, &conn_change_time);
  SendDefaultServiceSignal(kFakeEthernetServicePath);

  // Query the connection status, ensure last change time reported correctly.
  UmTestUtils::ExpectVariableHasValue(true, provider_->var_is_connected());
  UmTestUtils::ExpectVariableHasValue(conn_change_time,
                                      provider_->var_conn_last_changed());
}

// Make sure that the provider initializes correctly even if shill is not
// responding, that variables can be obtained, and that they all return a null
// value (indicating that the underlying values were not set).
TEST_F(UmRealShillProviderTest, NoInitConnStatusReadBaseValues) {
  // Re-initialize the provider, no initial connection status response.
  Init(false);
  UmTestUtils::ExpectVariableNotSet(provider_->var_is_connected());
  UmTestUtils::ExpectVariableNotSet(provider_->var_conn_type());
  UmTestUtils::ExpectVariableNotSet(provider_->var_conn_last_changed());
}

// Test that, once a signal is received, the connection status and other info
// can be read correctly.
TEST_F(UmRealShillProviderTest, NoInitConnStatusReadConnTypeEthernet) {
  // Re-initialize the provider, no initial connection status response.
  Init(false);
  SetupConnectionAndTestType(kFakeEthernetServicePath,
                             kFakeEthernetServiceProxy,
                             shill::kTypeEthernet,
                             ConnectionType::kEthernet);
}

}  // namespace chromeos_update_manager
