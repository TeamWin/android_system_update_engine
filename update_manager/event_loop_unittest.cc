// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/event_loop.h"

#include <base/bind.h>
#include <gtest/gtest.h>

#include "update_engine/test_utils.h"

using base::Bind;
using base::TimeDelta;
using chromeos_update_engine::RunGMainLoopMaxIterations;
using chromeos_update_engine::RunGMainLoopUntil;

namespace {

// Sets the value of the passed pointer to true.
void SetTrue(bool* value) {
  *value = true;
}

bool GetBoolean(bool* value) {
  return *value;
}

}  // namespace

namespace chromeos_update_manager {

class EventLoopTest : public ::testing::Test {};

TEST(EventLoopTest, RunFromMainLoopTest) {
  bool called = false;
  EventId ev = RunFromMainLoop(Bind(SetTrue, &called));
  EXPECT_NE(0, ev);
  RunGMainLoopMaxIterations(100);
  EXPECT_TRUE(called);
}

// Tests that we can cancel events right after we schedule them.
TEST(EventLoopTest, RunFromMainLoopCancelTest) {
  bool called = false;
  EventId ev = RunFromMainLoop(Bind(SetTrue, &called));
  EXPECT_NE(0, ev);
  EXPECT_TRUE(CancelMainLoopEvent(ev));
  RunGMainLoopMaxIterations(100);
  EXPECT_FALSE(called);
}

TEST(EventLoopTest, RunFromMainLoopAfterTimeoutTest) {
  bool called = false;
  EventId ev = RunFromMainLoopAfterTimeout(Bind(SetTrue, &called),
                                           TimeDelta::FromSeconds(1));
  EXPECT_NE(0, ev);
  RunGMainLoopUntil(10000, Bind(GetBoolean, &called));
  // Check that the main loop finished before the 10 seconds timeout.
  EXPECT_TRUE(called);
}

}  // namespace chromeos_update_manager
