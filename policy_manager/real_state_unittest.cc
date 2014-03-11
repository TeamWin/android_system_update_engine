// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(garnold) Remove once shill DBus constants not needed.
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/mock_dbus_wrapper.h"
#include "update_engine/policy_manager/real_state.h"
#include "update_engine/policy_manager/pmtest_utils.h"
// TODO(garnold) Remove once we stop mocking DBus.
#include "update_engine/test_utils.h"

using chromeos_update_engine::FakeClock;
using chromeos_update_engine::GValueNewString;
using chromeos_update_engine::GValueFree;
using chromeos_update_engine::MockDBusWrapper;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

namespace {

// TODO(garnold) This whole section gets removed once we mock the shill provider
// itself in tests.

// Fake dbus-glib objects.
DBusGConnection* const kFakeConnection = reinterpret_cast<DBusGConnection*>(1);
DBusGProxy* const kFakeManagerProxy = reinterpret_cast<DBusGProxy*>(2);

}  // namespace

namespace chromeos_policy_manager {

TEST(PmRealStateTest, InitTest) {
  NiceMock<MockDBusWrapper> mock_dbus;
  FakeClock fake_clock;

  // TODO(garnold) Replace this low-level DBus injection with a high-level
  // mock shill provider.
  EXPECT_CALL(mock_dbus, BusGet(_, _))
      .WillOnce(Return(kFakeConnection));
  EXPECT_CALL(mock_dbus, ProxyNewForName(_, _, _, _))
      .WillOnce(Return(kFakeManagerProxy));
  EXPECT_CALL(mock_dbus, ProxyAddSignal_2(_, _, _, _))
      .WillOnce(Return());
  EXPECT_CALL(mock_dbus, ProxyConnectSignal(_, _, _, _, _))
      .WillOnce(Return());
  auto properties = g_hash_table_new_full(g_str_hash, g_str_equal, free,
                                          GValueFree);
  g_hash_table_insert(properties, strdup(shill::kDefaultServiceProperty),
                      GValueNewString("/"));
  EXPECT_CALL(mock_dbus, ProxyCall_0_1(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(g_hash_table_ref(properties)),
                      Return(true)));

  RealState state(&mock_dbus, &fake_clock);
  EXPECT_TRUE(state.Init());

  // TODO(garnold) Remove this, too.
  g_hash_table_unref(properties);

  // Check that the providers are being initialized.
  PMTEST_ASSERT_NOT_NULL(state.random_provider());
  PMTEST_EXPECT_NOT_NULL(state.random_provider()->var_seed());
}

}  // namespace chromeos_policy_manager
