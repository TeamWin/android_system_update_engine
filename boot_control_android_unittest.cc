//
// Copyright (C) 2018 The Android Open Source Project
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

#include "update_engine/boot_control_android.h"

#include <set>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <fs_mgr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libdm/dm.h>

#include "update_engine/dynamic_partition_test_utils.h"
#include "update_engine/mock_boot_control_hal.h"
#include "update_engine/mock_dynamic_partition_control.h"

using android::dm::DmDeviceState;
using android::hardware::Void;
using std::string;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::NiceMock;
using testing::Not;
using testing::Return;

namespace chromeos_update_engine {

class BootControlAndroidTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Fake init bootctl_
    bootctl_.module_ = new NiceMock<MockBootControlHal>();
    bootctl_.dynamic_control_ =
        std::make_unique<NiceMock<MockDynamicPartitionControl>>();

    ON_CALL(module(), getNumberSlots()).WillByDefault(Invoke([] {
      return kMaxNumSlots;
    }));
    ON_CALL(module(), getSuffix(_, _))
        .WillByDefault(Invoke([](auto slot, auto cb) {
          EXPECT_LE(slot, kMaxNumSlots);
          cb(slot < kMaxNumSlots ? kSlotSuffixes[slot] : "");
          return Void();
        }));

    ON_CALL(dynamicControl(), GetDynamicPartitionsFeatureFlag())
        .WillByDefault(Return(FeatureFlag(FeatureFlag::Value::LAUNCH)));
    ON_CALL(dynamicControl(), DeviceExists(_)).WillByDefault(Return(true));
    ON_CALL(dynamicControl(), GetDeviceDir(_))
        .WillByDefault(Invoke([](auto path) {
          *path = kFakeDevicePath;
          return true;
        }));
    ON_CALL(dynamicControl(), GetDmDevicePathByName(_, _))
        .WillByDefault(Invoke([](auto partition_name_suffix, auto device) {
          *device = GetDmDevice(partition_name_suffix);
          return true;
        }));

    ON_CALL(dynamicControl(), GetSuperPartitionName(_))
        .WillByDefault(Return(kFakeSuper));
  }

  std::string GetSuperDevice(uint32_t slot) {
    return GetDevice(dynamicControl().GetSuperPartitionName(slot));
  }

  // Return the mocked HAL module.
  NiceMock<MockBootControlHal>& module() {
    return static_cast<NiceMock<MockBootControlHal>&>(*bootctl_.module_);
  }

  // Return the mocked DynamicPartitionControlInterface.
  NiceMock<MockDynamicPartitionControl>& dynamicControl() {
    return static_cast<NiceMock<MockDynamicPartitionControl>&>(
        *bootctl_.dynamic_control_);
  }

  // Set the fake metadata to return when LoadMetadataBuilder is called on
  // |slot|.
  void SetMetadata(uint32_t slot, const PartitionSuffixSizes& sizes) {
    EXPECT_CALL(dynamicControl(),
                LoadMetadataBuilder(GetSuperDevice(slot), slot))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke([sizes](auto, auto) {
          return NewFakeMetadata(PartitionSuffixSizesToMetadata(sizes));
        }));
  }

  uint32_t source() { return slots_.source; }

  uint32_t target() { return slots_.target; }

  // Return partition names with suffix of source().
  string S(const string& name) { return name + kSlotSuffixes[source()]; }

  // Return partition names with suffix of target().
  string T(const string& name) { return name + kSlotSuffixes[target()]; }

  // Set source and target slots to use before testing.
  void SetSlots(const TestParam& slots) {
    slots_ = slots;

    ON_CALL(module(), getCurrentSlot()).WillByDefault(Invoke([this] {
      return source();
    }));
  }

  bool InitPartitionMetadata(uint32_t slot,
                             PartitionSizes partition_sizes,
                             bool update_metadata = true) {
    auto m = PartitionSizesToMetadata(partition_sizes);
    return bootctl_.InitPartitionMetadata(slot, m, update_metadata);
  }

  BootControlAndroid bootctl_;  // BootControlAndroid under test.
  TestParam slots_;
};

class BootControlAndroidTestP
    : public BootControlAndroidTest,
      public ::testing::WithParamInterface<TestParam> {
 public:
  void SetUp() override {
    BootControlAndroidTest::SetUp();
    SetSlots(GetParam());
  }
};

// Test applying retrofit update on a build with dynamic partitions enabled.
TEST_P(BootControlAndroidTestP,
       ApplyRetrofitUpdateOnDynamicPartitionsEnabledBuild) {
  SetMetadata(source(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 2_GiB},
               {T("vendor"), 1_GiB}});

  // Not calling through BootControlAndroidTest::InitPartitionMetadata(), since
  // we don't want any default group in the PartitionMetadata.
  EXPECT_TRUE(bootctl_.InitPartitionMetadata(target(), {}, true));

  // Should use dynamic source partitions.
  EXPECT_CALL(dynamicControl(), GetState(S("system")))
      .Times(1)
      .WillOnce(Return(DmDeviceState::ACTIVE));
  string system_device;
  EXPECT_TRUE(bootctl_.GetPartitionDevice("system", source(), &system_device));
  EXPECT_EQ(GetDmDevice(S("system")), system_device);

  // Should use static target partitions without querying dynamic control.
  EXPECT_CALL(dynamicControl(), GetState(T("system"))).Times(0);
  EXPECT_TRUE(bootctl_.GetPartitionDevice("system", target(), &system_device));
  EXPECT_EQ(GetDevice(T("system")), system_device);

  // Static partition "bar".
  EXPECT_CALL(dynamicControl(), GetState(S("bar"))).Times(0);
  std::string bar_device;
  EXPECT_TRUE(bootctl_.GetPartitionDevice("bar", source(), &bar_device));
  EXPECT_EQ(GetDevice(S("bar")), bar_device);

  EXPECT_CALL(dynamicControl(), GetState(T("bar"))).Times(0);
  EXPECT_TRUE(bootctl_.GetPartitionDevice("bar", target(), &bar_device));
  EXPECT_EQ(GetDevice(T("bar")), bar_device);
}

TEST_P(BootControlAndroidTestP, GetPartitionDeviceWhenResumingUpdate) {
  // Both of the two slots contain valid partition metadata, since this is
  // resuming an update.
  SetMetadata(source(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 2_GiB},
               {T("vendor"), 1_GiB}});
  SetMetadata(target(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 2_GiB},
               {T("vendor"), 1_GiB}});

  EXPECT_TRUE(InitPartitionMetadata(
      target(), {{"system", 2_GiB}, {"vendor", 1_GiB}}, false));

  // Dynamic partition "system".
  EXPECT_CALL(dynamicControl(), GetState(S("system")))
      .Times(1)
      .WillOnce(Return(DmDeviceState::ACTIVE));
  string system_device;
  EXPECT_TRUE(bootctl_.GetPartitionDevice("system", source(), &system_device));
  EXPECT_EQ(GetDmDevice(S("system")), system_device);

  EXPECT_CALL(dynamicControl(), GetState(T("system")))
      .Times(AnyNumber())
      .WillOnce(Return(DmDeviceState::ACTIVE));
  EXPECT_CALL(dynamicControl(),
              MapPartitionOnDeviceMapper(
                  GetSuperDevice(target()), T("system"), target(), _, _))
      .Times(AnyNumber())
      .WillRepeatedly(
          Invoke([](const auto&, const auto& name, auto, auto, auto* device) {
            *device = "/fake/remapped/" + name;
            return true;
          }));
  EXPECT_TRUE(bootctl_.GetPartitionDevice("system", target(), &system_device));
  EXPECT_EQ("/fake/remapped/" + T("system"), system_device);

  // Static partition "bar".
  EXPECT_CALL(dynamicControl(), GetState(S("bar"))).Times(0);
  std::string bar_device;
  EXPECT_TRUE(bootctl_.GetPartitionDevice("bar", source(), &bar_device));
  EXPECT_EQ(GetDevice(S("bar")), bar_device);

  EXPECT_CALL(dynamicControl(), GetState(T("bar"))).Times(0);
  EXPECT_TRUE(bootctl_.GetPartitionDevice("bar", target(), &bar_device));
  EXPECT_EQ(GetDevice(T("bar")), bar_device);
}

INSTANTIATE_TEST_CASE_P(BootControlAndroidTest,
                        BootControlAndroidTestP,
                        testing::Values(TestParam{0, 1}, TestParam{1, 0}));

TEST_F(BootControlAndroidTest, ApplyingToCurrentSlot) {
  SetSlots({1, 1});
  EXPECT_FALSE(InitPartitionMetadata(target(), {}))
      << "Should not be able to apply to current slot.";
}

}  // namespace chromeos_update_engine
