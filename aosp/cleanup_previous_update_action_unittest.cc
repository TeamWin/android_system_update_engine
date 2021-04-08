//
// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <algorithm>

#include <brillo/message_loops/fake_message_loop.h>
#include <gtest/gtest.h>
#include <libsnapshot/snapshot.h>
#include <libsnapshot/mock_snapshot.h>
#include <libsnapshot/mock_snapshot_merge_stats.h>

#include "update_engine/aosp/cleanup_previous_update_action.h"
#include "update_engine/common/mock_boot_control.h"
#include "update_engine/common/mock_dynamic_partition_control.h"
#include "update_engine/common/mock_prefs.h"

namespace chromeos_update_engine {

using android::snapshot::AutoDevice;
using android::snapshot::MockSnapshotManager;
using android::snapshot::MockSnapshotMergeStats;
using android::snapshot::UpdateState;
using testing::_;
using testing::AtLeast;
using testing::Return;

class MockCleanupPreviousUpdateActionDelegate final
    : public CleanupPreviousUpdateActionDelegateInterface {
  MOCK_METHOD(void, OnCleanupProgressUpdate, (double), (override));
};

class MockActionProcessor : public ActionProcessor {
 public:
  MOCK_METHOD(void, ActionComplete, (AbstractAction*, ErrorCode), (override));
};

class MockAutoDevice : public AutoDevice {
 public:
  explicit MockAutoDevice(std::string name) : AutoDevice(name) {}
  ~MockAutoDevice() = default;
};

class CleanupPreviousUpdateActionTest : public ::testing::Test {
 public:
  void SetUp() override {
    ON_CALL(boot_control_, GetDynamicPartitionControl())
        .WillByDefault(Return(&dynamic_control_));
    ON_CALL(boot_control_, GetCurrentSlot()).WillByDefault(Return(0));
    ON_CALL(mock_snapshot_, GetSnapshotMergeStatsInstance())
        .WillByDefault(Return(&mock_stats_));
    action_.SetProcessor(&mock_processor_);
    loop_.SetAsCurrent();
  }

  constexpr static FeatureFlag LAUNCH{FeatureFlag::Value::LAUNCH};
  constexpr static FeatureFlag NONE{FeatureFlag::Value::NONE};
  MockSnapshotManager mock_snapshot_;
  MockPrefs mock_prefs_;
  MockBootControl boot_control_;
  MockDynamicPartitionControl dynamic_control_{};
  MockCleanupPreviousUpdateActionDelegate mock_delegate_;
  MockSnapshotMergeStats mock_stats_;
  MockActionProcessor mock_processor_;
  brillo::FakeMessageLoop loop_{nullptr};
  CleanupPreviousUpdateAction action_{
      &mock_prefs_, &boot_control_, &mock_snapshot_, &mock_delegate_};
};

TEST_F(CleanupPreviousUpdateActionTest, NonVabTest) {
  // Since VAB isn't even enabled, |GetSnapshotMergeStatsInstance| shouldn't be
  // called at all
  EXPECT_CALL(mock_snapshot_, GetSnapshotMergeStatsInstance()).Times(0);
  EXPECT_CALL(dynamic_control_, GetVirtualAbFeatureFlag())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(NONE));
  action_.PerformAction();
}

TEST_F(CleanupPreviousUpdateActionTest, VABSlotSuccessful) {
  // Expectaion: if VABC is enabled, Clenup action should call
  // |SnapshotMergeStats::Start()| to start merge, and wait for it to finish
  EXPECT_CALL(mock_snapshot_, GetSnapshotMergeStatsInstance())
      .Times(AtLeast(1));
  EXPECT_CALL(mock_snapshot_, EnsureMetadataMounted())
      .Times(AtLeast(1))
      .WillRepeatedly(
          []() { return std::make_unique<MockAutoDevice>("mock_device"); });
  EXPECT_CALL(dynamic_control_, GetVirtualAbFeatureFlag())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(LAUNCH));
  // CleanupPreviousUpdateAction should use whatever slot returned by
  // |GetCurrentSlot()|
  EXPECT_CALL(boot_control_, GetCurrentSlot())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(boot_control_, IsSlotMarkedSuccessful(1))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_snapshot_, ProcessUpdateState(_, _))
      .Times(AtLeast(2))
      .WillOnce(Return(UpdateState::Merging))
      .WillRepeatedly(Return(UpdateState::MergeCompleted));
  EXPECT_CALL(mock_stats_, Start())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_processor_, ActionComplete(&action_, ErrorCode::kSuccess))
      .Times(1);
  action_.PerformAction();
  while (loop_.PendingTasks()) {
    ASSERT_TRUE(loop_.RunOnce(true));
  }
}

TEST_F(CleanupPreviousUpdateActionTest, VabSlotNotReady) {
  // Cleanup action should repeatly query boot control until the slot is marked
  // successful.
  static constexpr auto MAX_TIMEPOINT =
      std::chrono::steady_clock::time_point::max();
  EXPECT_CALL(mock_snapshot_, GetSnapshotMergeStatsInstance())
      .Times(AtLeast(1));
  EXPECT_CALL(mock_snapshot_, EnsureMetadataMounted())
      .Times(AtLeast(1))
      .WillRepeatedly(
          []() { return std::make_unique<MockAutoDevice>("mock_device"); });
  EXPECT_CALL(dynamic_control_, GetVirtualAbFeatureFlag())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(LAUNCH));
  auto slot_success_time = MAX_TIMEPOINT;
  auto merge_start_time = MAX_TIMEPOINT;
  EXPECT_CALL(boot_control_, IsSlotMarkedSuccessful(_))
      .Times(AtLeast(3))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce([&slot_success_time]() {
        slot_success_time =
            std::min(slot_success_time, std::chrono::steady_clock::now());
        return true;
      });

  EXPECT_CALL(mock_stats_, Start())
      .Times(1)
      .WillRepeatedly([&merge_start_time]() {
        merge_start_time =
            std::min(merge_start_time, std::chrono::steady_clock::now());
        return true;
      });

  EXPECT_CALL(mock_snapshot_, ProcessUpdateState(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(UpdateState::MergeCompleted));
  EXPECT_CALL(mock_processor_, ActionComplete(&action_, ErrorCode::kSuccess))
      .Times(1);
  action_.PerformAction();
  while (loop_.PendingTasks()) {
    ASSERT_TRUE(loop_.RunOnce(true));
  }
  ASSERT_LT(slot_success_time, merge_start_time)
      << "Merge should not be started until slot is marked successful";
}

}  // namespace chromeos_update_engine
