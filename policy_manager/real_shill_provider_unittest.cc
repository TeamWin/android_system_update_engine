// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <base/time.h>
#include <chromeos/dbus/service_constants.h>
#include <glib.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/mock_dbus_wrapper.h"
#include "update_engine/policy_manager/real_shill_provider.h"
#include "update_engine/policy_manager/pmtest_utils.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::FakeClock;
using chromeos_update_engine::MockDBusWrapper;
using std::pair;
using testing::_;
using testing::NiceMock;
using testing::Return;
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

// Allocates, initializes and returns a string GValue object.
GValue* GValueNewString(const char* str) {
  GValue* gval = new GValue();
  g_value_init(gval, G_TYPE_STRING);
  g_value_set_string(gval, str);
  return gval;
}

// Frees a GValue object and its allocated resources.
void GValueFree(gpointer arg) {
  auto gval = reinterpret_cast<GValue*>(arg);
  g_value_unset(gval);
  delete gval;
}

}  // namespace

namespace chromeos_policy_manager {

class PmRealShillProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // The provider initializes correctly.
    fake_clock_.SetWallclockTime(init_time_);
    // A DBus connection should only be obtained once.
    EXPECT_CALL(mock_dbus_, BusGet(_, _)).WillOnce(Return(kFakeConnection));
    provider_.reset(new RealShillProvider(&mock_dbus_, &fake_clock_));
    PMTEST_ASSERT_NOT_NULL(provider_.get());
    // A manager proxy should only be obtained once.
    EXPECT_CALL(mock_dbus_, ProxyNewForName(
            kFakeConnection, StrEq(shill::kFlimflamServiceName),
            StrEq(shill::kFlimflamServicePath),
            StrEq(shill::kFlimflamManagerInterface)))
        .WillOnce(Return(kFakeManagerProxy));
    ASSERT_TRUE(provider_->Init());
  }

  virtual void TearDown() {
    // Make sure the manager proxy gets unreffed (once).
    EXPECT_CALL(mock_dbus_, ProxyUnref(kFakeManagerProxy)).WillOnce(Return());
    provider_.reset();
    // Verify and clear all expectations.
    testing::Mock::VerifyAndClear(&mock_dbus_);
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
                                ShillConnType expected_conn_type) {
    // Mock logic for returning a default service path.
    pair<const char*, const char*> manager_pairs[] = {
      {shill::kDefaultServiceProperty, service_path},
    };
    auto manager_properties = MockGetProperties(
        kFakeManagerProxy, true, arraysize(manager_pairs), manager_pairs);

    // Query the connection status, ensure last change time reported correctly.
    const Time change_time = Time::Now();
    fake_clock_.SetWallclockTime(change_time);
    scoped_ptr<const bool> is_connected(
        provider_->var_is_connected()->GetValue(default_timeout_, NULL));
    PMTEST_ASSERT_NOT_NULL(is_connected.get());
    EXPECT_TRUE(*is_connected);

    scoped_ptr<const Time> conn_last_changed_1(
        provider_->var_conn_last_changed()->GetValue(default_timeout_, NULL));
    PMTEST_ASSERT_NOT_NULL(conn_last_changed_1.get());
    EXPECT_EQ(change_time, *conn_last_changed_1);

    // Mock logic for querying the type of the default service.
    EXPECT_CALL(mock_dbus_, ProxyNewForName(
            kFakeConnection, StrEq(shill::kFlimflamServiceName),
            StrEq(service_path),
            StrEq(shill::kFlimflamServiceInterface)))
        .WillOnce(Return(service_proxy));
    EXPECT_CALL(mock_dbus_, ProxyUnref(service_proxy))
        .WillOnce(Return());
    pair<const char*, const char*> service_pairs[] = {
      {shill::kTypeProperty, shill_type_str},
    };
    auto service_properties = MockGetProperties(
        service_proxy, false, arraysize(service_pairs),
        service_pairs);

    // Query the connection type, ensure last change time reported correctly.
    fake_clock_.SetWallclockTime(change_time + TimeDelta::FromSeconds(5));
    scoped_ptr<const ShillConnType> conn_type(
        provider_->var_conn_type()->GetValue(default_timeout_, NULL));
    PMTEST_ASSERT_NOT_NULL(conn_type.get());
    EXPECT_EQ(expected_conn_type, *conn_type);

    scoped_ptr<const Time> conn_last_changed_2(
        provider_->var_conn_last_changed()->GetValue(default_timeout_, NULL));
    PMTEST_ASSERT_NOT_NULL(conn_last_changed_2.get());
    EXPECT_EQ(change_time, *conn_last_changed_2);

    // Release properties hash tables.
    g_hash_table_unref(service_properties);
    g_hash_table_unref(manager_properties);
  }

  const TimeDelta default_timeout_ = TimeDelta::FromSeconds(1);
  const Time init_time_ = Time::Now();
  StrictMock<MockDBusWrapper> mock_dbus_;
  FakeClock fake_clock_;
  scoped_ptr<RealShillProvider> provider_;
};

// Programs the mock DBus interface to indicate no default connection; this is
// in line with the default values the variables should be initialized with, and
// so the last changed time should not be updated. Also note that reading the
// connection type should not induce getting of a per-service proxy, as no
// default service is reported.
TEST_F(PmRealShillProviderTest, ReadDefaultValues) {
  // Mock logic for returning no default connection.
  pair<const char*, const char*> manager_pairs[] = {
    {shill::kDefaultServiceProperty, "/"},
  };
  auto manager_properties = MockGetProperties(
      kFakeManagerProxy, true, arraysize(manager_pairs), manager_pairs);

  // Query the provider variables.
  scoped_ptr<const bool> is_connected(
      provider_->var_is_connected()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(is_connected.get());
  EXPECT_FALSE(*is_connected);

  scoped_ptr<const ShillConnType> conn_type(
      provider_->var_conn_type()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NULL(conn_type.get());

  scoped_ptr<const Time> conn_last_changed(
      provider_->var_conn_last_changed()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(conn_last_changed.get());
  EXPECT_EQ(init_time_, *conn_last_changed);

  // Release properties hash table.
  g_hash_table_unref(manager_properties);
}

// Test that Ethernet connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaEthernet) {
  SetConnectionAndTestType(kFakeEthernetServicePath,
                           kFakeEthernetServiceProxy,
                           shill::kTypeEthernet,
                           kShillConnTypeEthernet);
}

// Test that Wifi connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaWifi) {
  SetConnectionAndTestType(kFakeWifiServicePath,
                           kFakeWifiServiceProxy,
                           shill::kTypeWifi,
                           kShillConnTypeWifi);
}

// Test that Wimax connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaWimax) {
  SetConnectionAndTestType(kFakeWimaxServicePath,
                           kFakeWimaxServiceProxy,
                           shill::kTypeWimax,
                           kShillConnTypeWimax);
}

// Test that Bluetooth connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaBluetooth) {
  SetConnectionAndTestType(kFakeBluetoothServicePath,
                           kFakeBluetoothServiceProxy,
                           shill::kTypeBluetooth,
                           kShillConnTypeBluetooth);
}

// Test that Cellular connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaCellular) {
  SetConnectionAndTestType(kFakeCellularServicePath,
                           kFakeCellularServiceProxy,
                           shill::kTypeCellular,
                           kShillConnTypeCellular);
}

// Test that an unknown connection is identified as such.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaUnknown) {
  SetConnectionAndTestType(kFakeUnknownServicePath,
                           kFakeUnknownServiceProxy,
                           "FooConnectionType",
                           kShillConnTypeUnknown);
}

// Tests that VPN connection is identified correctly.
TEST_F(PmRealShillProviderTest, ReadChangedValuesConnectedViaVpn) {
  // Mock logic for returning a default service path and its type.
  pair<const char*, const char*> manager_pairs[] = {
    {shill::kDefaultServiceProperty, kFakeVpnServicePath},
  };
  auto manager_properties = MockGetProperties(
      kFakeManagerProxy, false, arraysize(manager_pairs), manager_pairs);
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
  fake_clock_.SetWallclockTime(Time::Now());
  scoped_ptr<const ShillConnType> conn_type(
      provider_->var_conn_type()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(conn_type.get());
  EXPECT_EQ(kShillConnTypeWifi, *conn_type);

  scoped_ptr<const Time> conn_last_changed(
      provider_->var_conn_last_changed()->GetValue(default_timeout_, NULL));
  PMTEST_ASSERT_NOT_NULL(conn_last_changed.get());
  EXPECT_EQ(fake_clock_.GetWallclockTime(), *conn_last_changed);

  // Release properties hash tables.
  g_hash_table_unref(service_properties);
  g_hash_table_unref(manager_properties);
}

}  // namespace chromeos_policy_manager
