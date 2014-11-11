// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/connection_manager.h"

#include <set>
#include <string>

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "update_engine/fake_system_state.h"
#include "update_engine/mock_dbus_wrapper.h"
#include "update_engine/test_utils.h"

using std::set;
using std::string;
using testing::AnyNumber;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::_;

namespace chromeos_update_engine {

class ConnectionManagerTest : public ::testing::Test {
 public:
  ConnectionManagerTest()
      : kMockFlimFlamManagerProxy_(nullptr),
        kMockFlimFlamServiceProxy_(nullptr),
        kServicePath_(nullptr),
        cmut_(&fake_system_state_) {
    fake_system_state_.set_connection_manager(&cmut_);
  }

 protected:
  void SetupMocks(const char* service_path);
  void SetManagerReply(const char* reply_value, const GType& reply_type);

  // Sets the |service_type| Type and the |physical_technology|
  // PhysicalTechnology properties in the mocked service. If a null
  // |physical_technology| is passed, the property is not set (not present).
  void SetServiceReply(const char* service_type,
                       const char* physical_technology,
                       const char* service_tethering);
  void TestWithServiceType(
      const char* service_type,
      const char* physical_technology,
      NetworkConnectionType expected_type);
  void TestWithServiceTethering(
      const char* service_tethering,
      NetworkTethering expected_tethering);

  static const char* kGetPropertiesMethod;
  DBusGProxy* kMockFlimFlamManagerProxy_;
  DBusGProxy* kMockFlimFlamServiceProxy_;
  DBusGConnection* kMockSystemBus_;
  const char* kServicePath_;
  testing::StrictMock<MockDBusWrapper> dbus_iface_;
  ConnectionManager cmut_;  // ConnectionManager under test.
  FakeSystemState fake_system_state_;
};

// static
const char* ConnectionManagerTest::kGetPropertiesMethod = "GetProperties";

void ConnectionManagerTest::SetupMocks(const char* service_path) {
  int number = 1;
  kMockSystemBus_ = reinterpret_cast<DBusGConnection*>(number++);
  kMockFlimFlamManagerProxy_ = reinterpret_cast<DBusGProxy*>(number++);
  kMockFlimFlamServiceProxy_ = reinterpret_cast<DBusGProxy*>(number++);
  ASSERT_NE(kMockSystemBus_, static_cast<DBusGConnection*>(nullptr));

  kServicePath_ = service_path;

  ON_CALL(dbus_iface_, BusGet(DBUS_BUS_SYSTEM, _))
      .WillByDefault(Return(kMockSystemBus_));
  EXPECT_CALL(dbus_iface_, BusGet(DBUS_BUS_SYSTEM, _))
      .Times(AnyNumber());
}

void ConnectionManagerTest::SetManagerReply(const char *reply_value,
                                            const GType& reply_type) {
  ASSERT_TRUE(dbus_g_type_is_collection(reply_type));

  // Create the GPtrArray array holding the |reply_value| pointer. The
  // |reply_value| string is duplicated because it should be mutable on the
  // interface and is because dbus-glib collections will g_free() each element
  // of the GPtrArray automatically when the |array_as_value| GValue is unset.
  // The g_strdup() is not being leaked.
  GPtrArray* array = g_ptr_array_new();
  ASSERT_NE(nullptr, array);
  g_ptr_array_add(array, g_strdup(reply_value));

  GValue* array_as_value = g_new0(GValue, 1);
  EXPECT_EQ(array_as_value, g_value_init(array_as_value, reply_type));
  g_value_take_boxed(array_as_value, array);

  // Initialize return value for D-Bus call to Manager object, which is a
  // hash table of static strings (char*) in GValue* containing a single array.
  GHashTable* manager_hash_table = g_hash_table_new_full(
      g_str_hash, g_str_equal,
      nullptr,  // no key_destroy_func because keys are static.
      test_utils::GValueFree);  // value_destroy_func
  g_hash_table_insert(manager_hash_table,
                      const_cast<char*>("Services"),
                      array_as_value);

  // Plumb return value into mock object.
  EXPECT_CALL(dbus_iface_, ProxyCall_0_1(kMockFlimFlamManagerProxy_,
                                         StrEq(kGetPropertiesMethod),
                                         _, _))
      .WillOnce(DoAll(SetArgumentPointee<3>(manager_hash_table), Return(TRUE)));

  // Set other expectations.
  EXPECT_CALL(dbus_iface_,
              ProxyNewForName(kMockSystemBus_,
                              StrEq(shill::kFlimflamServiceName),
                              StrEq(shill::kFlimflamServicePath),
                              StrEq(shill::kFlimflamManagerInterface)))
      .WillOnce(Return(kMockFlimFlamManagerProxy_));
  EXPECT_CALL(dbus_iface_, ProxyUnref(kMockFlimFlamManagerProxy_));
  EXPECT_CALL(dbus_iface_, BusGet(DBUS_BUS_SYSTEM, _))
      .RetiresOnSaturation();
}

void ConnectionManagerTest::SetServiceReply(const char* service_type,
                                            const char* physical_technology,
                                            const char* service_tethering) {
  // Initialize return value for D-Bus call to Service object, which is a
  // hash table of static strings (char*) in GValue*.
  GHashTable* service_hash_table = g_hash_table_new_full(
      g_str_hash, g_str_equal,
      nullptr,  // no key_destroy_func because keys are static.
      test_utils::GValueFree);  // value_destroy_func
  GValue* service_type_value = test_utils::GValueNewString(service_type);
  g_hash_table_insert(service_hash_table,
                      const_cast<char*>("Type"),
                      service_type_value);

  if (physical_technology) {
    GValue* physical_technology_value =
        test_utils::GValueNewString(physical_technology);
    g_hash_table_insert(service_hash_table,
                        const_cast<char*>("PhysicalTechnology"),
                        physical_technology_value);
  }

  if (service_tethering) {
    GValue* service_tethering_value =
        test_utils::GValueNewString(service_tethering);
    g_hash_table_insert(service_hash_table,
                        const_cast<char*>("Tethering"),
                        service_tethering_value);
  }

  // Plumb return value into mock object.
  EXPECT_CALL(dbus_iface_, ProxyCall_0_1(kMockFlimFlamServiceProxy_,
                                         StrEq(kGetPropertiesMethod),
                                         _, _))
      .WillOnce(DoAll(SetArgumentPointee<3>(service_hash_table), Return(TRUE)));

  // Set other expectations.
  EXPECT_CALL(dbus_iface_,
              ProxyNewForName(kMockSystemBus_,
                              StrEq(shill::kFlimflamServiceName),
                              StrEq(kServicePath_),
                              StrEq(shill::kFlimflamServiceInterface)))
      .WillOnce(Return(kMockFlimFlamServiceProxy_));
  EXPECT_CALL(dbus_iface_, ProxyUnref(kMockFlimFlamServiceProxy_));
  EXPECT_CALL(dbus_iface_, BusGet(DBUS_BUS_SYSTEM, _))
      .RetiresOnSaturation();
}

void ConnectionManagerTest::TestWithServiceType(
    const char* service_type,
    const char* physical_technology,
    NetworkConnectionType expected_type) {

  SetupMocks("/service/guest-network");
  SetManagerReply(kServicePath_, DBUS_TYPE_G_OBJECT_PATH_ARRAY);
  SetServiceReply(service_type, physical_technology,
                  shill::kTetheringNotDetectedState);

  NetworkConnectionType type;
  NetworkTethering tethering;
  EXPECT_TRUE(cmut_.GetConnectionProperties(&dbus_iface_, &type, &tethering));
  EXPECT_EQ(expected_type, type);
  testing::Mock::VerifyAndClearExpectations(&dbus_iface_);
}

void ConnectionManagerTest::TestWithServiceTethering(
    const char* service_tethering,
    NetworkTethering expected_tethering) {

  SetupMocks("/service/guest-network");
  SetManagerReply(kServicePath_, DBUS_TYPE_G_OBJECT_PATH_ARRAY);
  SetServiceReply(shill::kTypeWifi, nullptr, service_tethering);

  NetworkConnectionType type;
  NetworkTethering tethering;
  EXPECT_TRUE(cmut_.GetConnectionProperties(&dbus_iface_, &type, &tethering));
  EXPECT_EQ(expected_tethering, tethering);
}

TEST_F(ConnectionManagerTest, SimpleTest) {
  TestWithServiceType(shill::kTypeEthernet, nullptr, kNetEthernet);
  TestWithServiceType(shill::kTypeWifi, nullptr, kNetWifi);
  TestWithServiceType(shill::kTypeWimax, nullptr, kNetWimax);
  TestWithServiceType(shill::kTypeBluetooth, nullptr, kNetBluetooth);
  TestWithServiceType(shill::kTypeCellular, nullptr, kNetCellular);
}

TEST_F(ConnectionManagerTest, PhysicalTechnologyTest) {
  TestWithServiceType(shill::kTypeVPN, nullptr, kNetUnknown);
  TestWithServiceType(shill::kTypeVPN, shill::kTypeVPN, kNetUnknown);
  TestWithServiceType(shill::kTypeVPN, shill::kTypeWifi, kNetWifi);
  TestWithServiceType(shill::kTypeVPN, shill::kTypeWimax, kNetWimax);
}

TEST_F(ConnectionManagerTest, TetheringTest) {
  TestWithServiceTethering(shill::kTetheringConfirmedState,
                           NetworkTethering::kConfirmed);
  TestWithServiceTethering(shill::kTetheringNotDetectedState,
                           NetworkTethering::kNotDetected);
  TestWithServiceTethering(shill::kTetheringSuspectedState,
                           NetworkTethering::kSuspected);
  TestWithServiceTethering("I'm not a valid property value =)",
                           NetworkTethering::kUnknown);
}

TEST_F(ConnectionManagerTest, UnknownTest) {
  TestWithServiceType("foo", nullptr, kNetUnknown);
}

TEST_F(ConnectionManagerTest, AllowUpdatesOverEthernetTest) {
  // Updates over Ethernet are allowed even if there's no policy.
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetEthernet,
                                        NetworkTethering::kUnknown));
}

TEST_F(ConnectionManagerTest, AllowUpdatesOverWifiTest) {
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetWifi, NetworkTethering::kUnknown));
}

TEST_F(ConnectionManagerTest, AllowUpdatesOverWimaxTest) {
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetWimax,
                                        NetworkTethering::kUnknown));
}

TEST_F(ConnectionManagerTest, BlockUpdatesOverBluetoothTest) {
  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetBluetooth,
                                         NetworkTethering::kUnknown));
}

TEST_F(ConnectionManagerTest, AllowUpdatesOnlyOver3GPerPolicyTest) {
  policy::MockDevicePolicy allow_3g_policy;

  fake_system_state_.set_device_policy(&allow_3g_policy);

  // This test tests cellular (3G) being the only connection type being allowed.
  set<string> allowed_set;
  allowed_set.insert(cmut_.StringForConnectionType(kNetCellular));

  EXPECT_CALL(allow_3g_policy, GetAllowedConnectionTypesForUpdate(_))
      .Times(1)
      .WillOnce(DoAll(SetArgumentPointee<0>(allowed_set), Return(true)));

  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetCellular,
                                        NetworkTethering::kUnknown));
}

TEST_F(ConnectionManagerTest, AllowUpdatesOver3GAndOtherTypesPerPolicyTest) {
  policy::MockDevicePolicy allow_3g_policy;

  fake_system_state_.set_device_policy(&allow_3g_policy);

  // This test tests multiple connection types being allowed, with
  // 3G one among them. Only Cellular is currently enforced by the policy
  // setting, the others are ignored (see Bluetooth for example).
  set<string> allowed_set;
  allowed_set.insert(cmut_.StringForConnectionType(kNetCellular));
  allowed_set.insert(cmut_.StringForConnectionType(kNetBluetooth));

  EXPECT_CALL(allow_3g_policy, GetAllowedConnectionTypesForUpdate(_))
      .Times(3)
      .WillRepeatedly(DoAll(SetArgumentPointee<0>(allowed_set), Return(true)));

  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetEthernet,
                                        NetworkTethering::kUnknown));
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetEthernet,
                                        NetworkTethering::kNotDetected));
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetCellular,
                                        NetworkTethering::kUnknown));
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetWifi, NetworkTethering::kUnknown));
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetWimax, NetworkTethering::kUnknown));
  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetBluetooth,
                                         NetworkTethering::kUnknown));

  // Tethered networks are treated in the same way as Cellular networks and
  // thus allowed.
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetEthernet,
                                        NetworkTethering::kConfirmed));
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetWifi,
                                        NetworkTethering::kConfirmed));
}

TEST_F(ConnectionManagerTest, BlockUpdatesOverCellularByDefaultTest) {
  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetCellular,
                                         NetworkTethering::kUnknown));
}

TEST_F(ConnectionManagerTest, BlockUpdatesOverTetheredNetworkByDefaultTest) {
  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetWifi,
                                         NetworkTethering::kConfirmed));
  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetEthernet,
                                         NetworkTethering::kConfirmed));
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetWifi,
                                        NetworkTethering::kSuspected));
}

TEST_F(ConnectionManagerTest, BlockUpdatesOver3GPerPolicyTest) {
  policy::MockDevicePolicy block_3g_policy;

  fake_system_state_.set_device_policy(&block_3g_policy);

  // Test that updates for 3G are blocked while updates are allowed
  // over several other types.
  set<string> allowed_set;
  allowed_set.insert(cmut_.StringForConnectionType(kNetEthernet));
  allowed_set.insert(cmut_.StringForConnectionType(kNetWifi));
  allowed_set.insert(cmut_.StringForConnectionType(kNetWimax));

  EXPECT_CALL(block_3g_policy, GetAllowedConnectionTypesForUpdate(_))
      .Times(1)
      .WillOnce(DoAll(SetArgumentPointee<0>(allowed_set), Return(true)));

  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetCellular,
                                         NetworkTethering::kUnknown));
}

TEST_F(ConnectionManagerTest, BlockUpdatesOver3GIfErrorInPolicyFetchTest) {
  policy::MockDevicePolicy allow_3g_policy;

  fake_system_state_.set_device_policy(&allow_3g_policy);

  set<string> allowed_set;
  allowed_set.insert(cmut_.StringForConnectionType(kNetCellular));

  // Return false for GetAllowedConnectionTypesForUpdate and see
  // that updates are still blocked for 3G despite the value being in
  // the string set above.
  EXPECT_CALL(allow_3g_policy, GetAllowedConnectionTypesForUpdate(_))
      .Times(1)
      .WillOnce(DoAll(SetArgumentPointee<0>(allowed_set), Return(false)));

  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetCellular,
                                         NetworkTethering::kUnknown));
}

TEST_F(ConnectionManagerTest, UseUserPrefForUpdatesOverCellularIfNoPolicyTest) {
  policy::MockDevicePolicy no_policy;
  testing::NiceMock<MockPrefs>* prefs = fake_system_state_.mock_prefs();

  fake_system_state_.set_device_policy(&no_policy);

  // No setting enforced by the device policy, user prefs should be used.
  EXPECT_CALL(no_policy, GetAllowedConnectionTypesForUpdate(_))
      .Times(3)
      .WillRepeatedly(Return(false));

  // No user pref: block.
  EXPECT_CALL(*prefs, Exists(kPrefsUpdateOverCellularPermission))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetCellular,
                                         NetworkTethering::kUnknown));

  // Allow per user pref.
  EXPECT_CALL(*prefs, Exists(kPrefsUpdateOverCellularPermission))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*prefs, GetBoolean(kPrefsUpdateOverCellularPermission, _))
      .Times(1)
      .WillOnce(DoAll(SetArgumentPointee<1>(true), Return(true)));
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetCellular,
                                        NetworkTethering::kUnknown));

  // Block per user pref.
  EXPECT_CALL(*prefs, Exists(kPrefsUpdateOverCellularPermission))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*prefs, GetBoolean(kPrefsUpdateOverCellularPermission, _))
      .Times(1)
      .WillOnce(DoAll(SetArgumentPointee<1>(false), Return(true)));
  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetCellular,
                                         NetworkTethering::kUnknown));
}

TEST_F(ConnectionManagerTest, StringForConnectionTypeTest) {
  EXPECT_STREQ(shill::kTypeEthernet,
               cmut_.StringForConnectionType(kNetEthernet));
  EXPECT_STREQ(shill::kTypeWifi,
               cmut_.StringForConnectionType(kNetWifi));
  EXPECT_STREQ(shill::kTypeWimax,
               cmut_.StringForConnectionType(kNetWimax));
  EXPECT_STREQ(shill::kTypeBluetooth,
               cmut_.StringForConnectionType(kNetBluetooth));
  EXPECT_STREQ(shill::kTypeCellular,
               cmut_.StringForConnectionType(kNetCellular));
  EXPECT_STREQ("Unknown",
               cmut_.StringForConnectionType(kNetUnknown));
  EXPECT_STREQ("Unknown",
               cmut_.StringForConnectionType(
                   static_cast<NetworkConnectionType>(999999)));
}

TEST_F(ConnectionManagerTest, MalformedServiceList) {
  SetupMocks("/service/guest-network");
  SetManagerReply(kServicePath_, DBUS_TYPE_G_STRING_ARRAY);

  NetworkConnectionType type;
  NetworkTethering tethering;
  EXPECT_FALSE(cmut_.GetConnectionProperties(&dbus_iface_, &type, &tethering));
}

}  // namespace chromeos_update_engine
