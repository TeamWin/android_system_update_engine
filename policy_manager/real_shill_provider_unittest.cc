// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <glib.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/mock_dbus_wrapper.h"
#include "update_engine/policy_manager/real_shill_provider.h"
#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/test_utils.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::FakeClock;
using chromeos_update_engine::GValueNewString;
using chromeos_update_engine::GValueFree;
using chromeos_update_engine::MockDBusWrapper;
using std::pair;
using testing::_;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::StrEq;
using testing::StrictMock;

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

namespace chromeos_policy_manager {

class PmRealShillProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    provider_.reset(new RealShillProvider(&mock_dbus_, &fake_clock_));
    PMTEST_ASSERT_NOT_NULL(provider_.get());
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
        .WillOnce(DoAll(SaveArg<2>(&signal_handler_),
                        SaveArg<3>(&signal_data_),
                        Return()));
    // Default service should be checked once during initialization.
    pair<const char*, const char*> manager_pairs[] = {
      {shill::kDefaultServiceProperty, "/"},
    };
    auto manager_properties = MockGetProperties(kFakeManagerProxy, false,
                                                1, manager_pairs);
    // Check that provider initializes corrrectly.
    ASSERT_TRUE(provider_->Init());
    // Release properties hash table.
    g_hash_table_unref(manager_properties);
  }

  virtual void TearDown() {
    // Make sure that DBus resources get freed.
    EXPECT_CALL(mock_dbus_, ProxyDisconnectSignal(
            kFakeManagerProxy, StrEq(shill::kMonitorPropertyChanged),
            Eq(signal_handler_), Eq(signal_data_)))
        .WillOnce(Return());
    EXPECT_CALL(mock_dbus_, ProxyUnref(kFakeManagerProxy)).WillOnce(Return());
    provider_.reset();
    // Verify and clear all expectations.
    testing::Mock::VerifyAndClear(&mock_dbus_);
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

  // Sets up a mock "GetProperties" call on |proxy| that returns a hash table
  // containing |num_entries| entries formed by subsequent key/value pairs. Keys
  // and values are plain C strings (const char*). Mock will be expected to call
  // exactly once, unless |allow_multiple_calls| is true, in which case it's
  // allowed to be called multiple times. Returns a pointer to a newly allocated
  // hash table, which should be unreffed with g_hash_table_unref() when done.
  GHashTable* MockGetProperties(DBusGProxy* proxy, bool allow_multiple_calls,
                                size_t num_entries,
                                pair<const char*, const char*>* key_val_pairs) {
    // Allocate and populate the hash table.
    auto properties = g_hash_table_new_full(g_str_hash, g_str_equal, free,
                                            GValueFree);
    for (size_t i = 0; i < num_entries; i++) {
      g_hash_table_insert(properties, strdup(key_val_pairs[i].first),
                          GValueNewString(key_val_pairs[i].second));
    }

    // Set mock expectation and actions.
    auto action = DoAll(SetArgPointee<3>(g_hash_table_ref(properties)),
                        Return(true));
    if (allow_multiple_calls) {
      EXPECT_CALL(mock_dbus_, ProxyCall_0_1(
              proxy, StrEq(shill::kGetPropertiesFunction), _, _))
          .WillRepeatedly(action);
    } else {
      EXPECT_CALL(mock_dbus_, ProxyCall_0_1(
              proxy, StrEq(shill::kGetPropertiesFunction), _, _))
          .WillOnce(action);
    }

    return properties;
  }

  // Programs the mock DBus interface to return an non-VPN service and ensures
  // that the shill provider reads it correctly, including updating the last
  // changed time on each of the queries.
  void SetConnectionAndTestType(const char* service_path,
                                DBusGProxy* service_proxy,
                                const char* shill_type_str,
                                ConnectionType expected_conn_type) {
    // Send a signal about a new default service.
    auto callback = reinterpret_cast<ShillConnector::PropertyChangedHandler>(
        signal_handler_);
    auto default_service_gval = GValueNewString(service_path);
    Time conn_change_time = ConnChangedTime();
    fake_clock_.SetWallclockTime(conn_change_time);
    callback(kFakeManagerProxy, shill::kDefaultServiceProperty,
             default_service_gval, signal_data_);
    fake_clock_.SetWallclockTime(conn_change_time + TimeDelta::FromSeconds(5));
    GValueFree(default_service_gval);

    // Query the connection status, ensure last change time reported correctly.
    scoped_ptr<const bool> is_connected(
        provider_->var_is_connected()->GetValue(default_timeout_, NULL));
    PMTEST_ASSERT_NOT_NULL(is_connected.get());
    EXPECT_TRUE(*is_connected);

    scoped_ptr<const Time> conn_last_changed_1(
        provider_->var_conn_last_changed()->GetValue(default_timeout_, NULL));
    PMTEST_ASSERT_NOT_NULL(conn_last_changed_1.get());
    EXPECT_EQ(conn_change_time, *conn_last_changed_1);

    // Mock logic for querying the type of the default service.
    EXPECT_CALL(mock_dbus_, ProxyNewForName(
            kFakeConnection, StrEq(shill::kFlimflamServiceName),
            StrEq(service_path),
            StrEq(shill::kFlimflamServiceInterface)))
        .WillOnce(Return(service_proxy));
    EXPECT_CALL(mock_dbus_,
                ProxyUnref(service_proxy)).WillOnce(Return());
    pair<const char*, const char*> service_pairs[] = {
      {shill::kTypeProperty, shill_type_str},
    };
    auto service_properties = MockGetProperties(
        service_proxy, false,
        arraysize(service_pairs), service_pairs);

    // Query the connection type, ensure last change time did not change.
    scoped_ptr<const ConnectionType> conn_type(
        provider_->var_conn_type()->GetValue(default_timeout_, NULL));
    PMTEST_ASSERT_NOT_NULL(conn_type.get());
    EXPECT_EQ(expected_conn_type, *conn_type);

    scoped_ptr<const Time> conn_last_changed_2(
        provider_->var_conn_last_changed()->GetValue(default_timeout_, NULL));
    PMTEST_ASSERT_NOT_NULL(conn_last_changed_2.get());
    EXPECT_EQ(*conn_last_changed_1, *conn_last_changed_2);

    // Release properties hash tables.
    g_hash_table_unref(service_properties);
  }

  const TimeDelta default_timeout_ = TimeDelta::FromSeconds(1);
  StrictMock<MockDBusWrapper> mock_dbus_;
  FakeClock fake_clock_;
  scoped_ptr<RealShillProvider> provider_;
  GCallback signal_handler_;
  // FIXME void (*signal_handler_)(DBusGProxy*, const char*, GValue*, void*);
  void* signal_data_;
};

// Query the connection status, type and time last changed, as they were set
// during initialization.
TEST_F(PmRealShillProviderTest, ReadDefaultValues) {
  // Query the provider variables.
  scoped_ptr<const bool> is_connected(
      provider_->var_is_connected()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(is_connected.get());
  EXPECT_FALSE(*is_connected);

  scoped_ptr<const ConnectionType> conn_type(
      provider_->var_conn_type()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NULL(conn_type.get());

  scoped_ptr<const Time> conn_last_changed(
      provider_->var_conn_last_changed()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(conn_last_changed.get());
  EXPECT_EQ(InitTime(), *conn_last_changed);
}

// Test that Ethernet connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaEthernet) {
  SetConnectionAndTestType(kFakeEthernetServicePath,
                           kFakeEthernetServiceProxy,
                           shill::kTypeEthernet,
                           ConnectionType::kEthernet);
}

// Test that Wifi connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaWifi) {
  SetConnectionAndTestType(kFakeWifiServicePath,
                           kFakeWifiServiceProxy,
                           shill::kTypeWifi,
                           ConnectionType::kWifi);
}

// Test that Wimax connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaWimax) {
  SetConnectionAndTestType(kFakeWimaxServicePath,
                           kFakeWimaxServiceProxy,
                           shill::kTypeWimax,
                           ConnectionType::kWimax);
}

// Test that Bluetooth connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaBluetooth) {
  SetConnectionAndTestType(kFakeBluetoothServicePath,
                           kFakeBluetoothServiceProxy,
                           shill::kTypeBluetooth,
                           ConnectionType::kBluetooth);
}

// Test that Cellular connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaCellular) {
  SetConnectionAndTestType(kFakeCellularServicePath,
                           kFakeCellularServiceProxy,
                           shill::kTypeCellular,
                           ConnectionType::kCellular);
}

// Test that an unknown connection is identified as such.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaUnknown) {
  SetConnectionAndTestType(kFakeUnknownServicePath,
                           kFakeUnknownServiceProxy,
                           "FooConnectionType",
                           ConnectionType::kUnknown);
}

// Tests that VPN connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaVpn) {
  // Send a signal about a new default service.
  auto callback = reinterpret_cast<ShillConnector::PropertyChangedHandler>(
      signal_handler_);
  auto default_service_gval = GValueNewString(kFakeVpnServicePath);
  Time conn_change_time = ConnChangedTime();
  fake_clock_.SetWallclockTime(conn_change_time);
  callback(kFakeManagerProxy, shill::kDefaultServiceProperty,
           default_service_gval, signal_data_);
  fake_clock_.SetWallclockTime(conn_change_time + TimeDelta::FromSeconds(5));
  GValueFree(default_service_gval);

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
  auto service_properties = MockGetProperties(
      kFakeVpnServiceProxy, false, arraysize(service_pairs), service_pairs);

  // Query the connection type, ensure last change time reported correctly.
  scoped_ptr<const ConnectionType> conn_type(
      provider_->var_conn_type()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(conn_type.get());
  EXPECT_EQ(ConnectionType::kWifi, *conn_type);

  scoped_ptr<const Time> conn_last_changed(
      provider_->var_conn_last_changed()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(conn_last_changed.get());
  EXPECT_EQ(conn_change_time, *conn_last_changed);

  // Release properties hash tables.
  g_hash_table_unref(service_properties);
}

// Fake two DBus signal prompting about a default connection change, but
// otherwise give the same service path. Check connection status and the time is
// was last changed, make sure it is the same time when the first signal was
// sent (and not the second).
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedTwoSignals) {
  // Send a default service signal twice, advancing the clock in between.
  auto callback = reinterpret_cast<ShillConnector::PropertyChangedHandler>(
      signal_handler_);
  auto default_service_gval = GValueNewString(kFakeEthernetServicePath);
  Time conn_change_time = ConnChangedTime();
  fake_clock_.SetWallclockTime(conn_change_time);
  callback(kFakeManagerProxy, shill::kDefaultServiceProperty,
           default_service_gval, signal_data_);
  fake_clock_.SetWallclockTime(conn_change_time + TimeDelta::FromSeconds(5));
  callback(kFakeManagerProxy, shill::kDefaultServiceProperty,
           default_service_gval, signal_data_);
  GValueFree(default_service_gval);

  // Query the connection status, ensure last change time reported correctly.
  scoped_ptr<const bool> is_connected(
      provider_->var_is_connected()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(is_connected.get());
  EXPECT_TRUE(*is_connected);

  scoped_ptr<const Time> conn_last_changed(
      provider_->var_conn_last_changed()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(conn_last_changed.get());
  EXPECT_EQ(conn_change_time, *conn_last_changed);
}

}  // namespace chromeos_policy_manager
