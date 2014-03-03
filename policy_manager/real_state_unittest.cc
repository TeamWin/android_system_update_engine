// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/mock_dbus_wrapper.h"
#include "update_engine/policy_manager/real_state.h"
#include "update_engine/policy_manager/pmtest_utils.h"

using chromeos_update_engine::FakeClock;
using chromeos_update_engine::MockDBusWrapper;
using testing::_;
using testing::NiceMock;
using testing::Return;

namespace {

DBusGConnection* const kFakeConnection = reinterpret_cast<DBusGConnection*>(1);

}  // namespace

namespace chromeos_policy_manager {

TEST(PmRealStateTest, InitTest) {
  NiceMock<MockDBusWrapper> mock_dbus;
  FakeClock fake_clock;
  EXPECT_CALL(mock_dbus, BusGet(_, _)).WillOnce(Return(kFakeConnection));
  RealState state(&mock_dbus, &fake_clock);
  EXPECT_TRUE(state.Init());
  // Check that the providers are being initialized.
  PMTEST_ASSERT_NOT_NULL(state.random_provider());
  PMTEST_EXPECT_NOT_NULL(state.random_provider()->var_seed());
}

}  // namespace chromeos_policy_manager
