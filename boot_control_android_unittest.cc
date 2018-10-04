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

#include <android-base/strings.h>
#include <fs_mgr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "update_engine/mock_boot_control_hal.h"
#include "update_engine/mock_dynamic_partition_control.h"

using android::base::Join;
using android::fs_mgr::MetadataBuilder;
using android::hardware::Void;
using testing::_;
using testing::AnyNumber;
using testing::Contains;
using testing::Eq;
using testing::Invoke;
using testing::Key;
using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::NiceMock;
using testing::Return;

namespace chromeos_update_engine {

constexpr const uint32_t kMaxNumSlots = 2;
constexpr const char* kSlotSuffixes[kMaxNumSlots] = {"_a", "_b"};
constexpr const char* kFakeDevicePath = "/fake/dev/path/";
constexpr const char* kFakeMappedPath = "/fake/mapped/path/";
constexpr const uint32_t kFakeMetadataSize = 65536;

// A map describing the size of each partition.
using PartitionSizes = std::map<std::string, uint64_t>;

// C++ standards do not allow uint64_t (aka unsigned long) to be the parameter
// of user-defined literal operators.
unsigned long long operator"" _MiB(unsigned long long x) {  // NOLINT
  return x << 20;
}
unsigned long long operator"" _GiB(unsigned long long x) {  // NOLINT
  return x << 30;
}

template <typename U, typename V>
std::ostream& operator<<(std::ostream& os, const std::map<U, V>& param) {
  os << "{";
  bool first = true;
  for (const auto& pair : param) {
    if (!first)
      os << ", ";
    os << pair.first << ":" << pair.second;
    first = false;
  }
  return os << "}";
}

inline std::string GetDevice(const std::string& name) {
  return kFakeDevicePath + name;
}
inline std::string GetSuperDevice() {
  return GetDevice(fs_mgr_get_super_partition_name());
}

struct TestParam {
  uint32_t source;
  uint32_t target;
};
std::ostream& operator<<(std::ostream& os, const TestParam& param) {
  return os << "{source: " << param.source << ", target:" << param.target
            << "}";
}

std::unique_ptr<MetadataBuilder> NewFakeMetadata(const PartitionSizes& sizes) {
  auto builder = MetadataBuilder::New(10_GiB, kFakeMetadataSize, kMaxNumSlots);
  EXPECT_NE(nullptr, builder);
  if (builder == nullptr)
    return nullptr;
  for (const auto& pair : sizes) {
    auto p = builder->AddPartition(pair.first, 0 /* attr */);
    EXPECT_TRUE(p && builder->ResizePartition(p, pair.second));
  }
  return builder;
}

class MetadataMatcher : public MatcherInterface<MetadataBuilder*> {
 public:
  explicit MetadataMatcher(const PartitionSizes& partition_sizes)
      : partition_sizes_(partition_sizes) {}
  bool MatchAndExplain(MetadataBuilder* metadata,
                       MatchResultListener* listener) const override {
    bool success = true;
    for (const auto& pair : partition_sizes_) {
      auto p = metadata->FindPartition(pair.first);
      if (p == nullptr) {
        if (success)
          *listener << "; ";
        *listener << "No partition " << pair.first;
        success = false;
        continue;
      }
      if (p->size() != pair.second) {
        if (success)
          *listener << "; ";
        *listener << "Partition " << pair.first << " has size " << p->size()
                  << ", expected " << pair.second;
        success = false;
      }
    }
    return success;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "expect: " << partition_sizes_;
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "expect not: " << partition_sizes_;
  }

 private:
  PartitionSizes partition_sizes_;
};

inline Matcher<MetadataBuilder*> MetadataMatches(
    const PartitionSizes& partition_sizes) {
  return MakeMatcher(new MetadataMatcher(partition_sizes));
}

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

    ON_CALL(dynamicControl(), IsDynamicPartitionsEnabled())
        .WillByDefault(Return(true));
    ON_CALL(dynamicControl(), GetDeviceDir(_))
        .WillByDefault(Invoke([](auto path) {
          *path = kFakeDevicePath;
          return true;
        }));
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
  void SetMetadata(uint32_t slot, const PartitionSizes& sizes) {
    EXPECT_CALL(dynamicControl(), LoadMetadataBuilder(GetSuperDevice(), slot))
        .WillOnce(
            Invoke([sizes](auto, auto) { return NewFakeMetadata(sizes); }));
  }

  // Expect that MapPartitionOnDeviceMapper is called on target() metadata slot
  // with each partition in |partitions|.
  void ExpectMap(const std::set<std::string>& partitions) {
    // Error when MapPartitionOnDeviceMapper is called on unknown arguments.
    ON_CALL(dynamicControl(), MapPartitionOnDeviceMapper(_, _, _, _))
        .WillByDefault(Return(false));

    for (const auto& partition : partitions) {
      EXPECT_CALL(
          dynamicControl(),
          MapPartitionOnDeviceMapper(GetSuperDevice(), partition, target(), _))
          .WillOnce(Invoke([this](auto, auto partition, auto, auto path) {
            auto it = mapped_devices_.find(partition);
            if (it != mapped_devices_.end()) {
              *path = it->second;
              return true;
            }
            mapped_devices_[partition] = *path = kFakeMappedPath + partition;
            return true;
          }));
    }
  }

  // Expect that UnmapPartitionOnDeviceMapper is called on target() metadata
  // slot with each partition in |partitions|.
  void ExpectUnmap(const std::set<std::string>& partitions) {
    // Error when UnmapPartitionOnDeviceMapper is called on unknown arguments.
    ON_CALL(dynamicControl(), UnmapPartitionOnDeviceMapper(_, _))
        .WillByDefault(Return(false));

    for (const auto& partition : partitions) {
      EXPECT_CALL(dynamicControl(), UnmapPartitionOnDeviceMapper(partition, _))
          .WillOnce(Invoke([this](auto partition, auto) {
            mapped_devices_.erase(partition);
            return true;
          }));
    }
  }

  void ExpectRemap(const std::set<std::string>& partitions) {
    ExpectUnmap(partitions);
    ExpectMap(partitions);
  }

  void ExpectDevicesAreMapped(const std::set<std::string>& partitions) {
    ASSERT_EQ(partitions.size(), mapped_devices_.size());
    for (const auto& partition : partitions) {
      EXPECT_THAT(mapped_devices_, Contains(Key(Eq(partition))))
          << "Expect that " << partition << " is mapped, but it is not.";
    }
  }

  uint32_t source() { return slots_.source; }

  uint32_t target() { return slots_.target; }

  // Return partition names with suffix of source().
  std::string S(const std::string& name) {
    return name + std::string(kSlotSuffixes[source()]);
  }

  // Return partition names with suffix of target().
  std::string T(const std::string& name) {
    return name + std::string(kSlotSuffixes[target()]);
  }

  // Set source and target slots to use before testing.
  void SetSlots(const TestParam& slots) {
    slots_ = slots;

    ON_CALL(module(), getCurrentSlot()).WillByDefault(Invoke([this] {
      return source();
    }));
    // Should not store metadata to source slot.
    EXPECT_CALL(dynamicControl(), StoreMetadata(GetSuperDevice(), _, source()))
        .Times(0);
  }

  BootControlAndroid bootctl_;  // BootControlAndroid under test.
  TestParam slots_;
  // mapped devices through MapPartitionOnDeviceMapper.
  std::map<std::string, std::string> mapped_devices_;
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

// Test no resize if no dynamic partitions at all.
TEST_P(BootControlAndroidTestP, NoResizeIfNoDynamicPartitions) {
  SetMetadata(source(), {});
  SetMetadata(target(), {});
  // Should not need to resize and store metadata
  EXPECT_CALL(dynamicControl(), StoreMetadata(GetSuperDevice(), _, target()))
      .Times(0);
  EXPECT_CALL(dynamicControl(), DeviceExists(Eq(GetDevice("static_a"))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(dynamicControl(), DeviceExists(Eq(GetDevice("static_b"))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(target(), {{"static", 1_GiB}}));
  ExpectDevicesAreMapped({});
}

// Test no resize if update manifest does not contain any dynamic partitions
TEST_P(BootControlAndroidTestP, NoResizeIfEmptyMetadata) {
  SetMetadata(source(),
              {{S("system"), 4_GiB},
               {S("vendor"), 100_MiB},
               {T("system"), 3_GiB},
               {T("vendor"), 150_MiB}});
  SetMetadata(target(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 3_GiB},
               {T("vendor"), 150_MiB}});
  // Should not need to resize and store metadata
  EXPECT_CALL(dynamicControl(), StoreMetadata(GetSuperDevice(), _, target()))
      .Times(0);
  EXPECT_CALL(dynamicControl(), DeviceExists(Eq(GetDevice("static_a"))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(dynamicControl(), DeviceExists(Eq(GetDevice("static_b"))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(target(), {{"static", 1_GiB}}));
  ExpectDevicesAreMapped({});
}

// Do not resize if manifest size matches size in target metadata. When resuming
// from an update, do not redo the resize if not needed.
TEST_P(BootControlAndroidTestP, NoResizeIfSizeMatchWhenResizing) {
  SetMetadata(source(), {{S("system"), 2_GiB}, {S("vendor"), 1_GiB}});
  SetMetadata(target(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 3_GiB},
               {T("vendor"), 1_GiB}});
  // Should not need to resize and store metadata
  EXPECT_CALL(dynamicControl(), StoreMetadata(GetSuperDevice(), _, target()))
      .Times(0);
  ExpectRemap({T("system"), T("vendor")});

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(
      target(), {{"system", 3_GiB}, {"vendor", 1_GiB}}));
  ExpectDevicesAreMapped({T("system"), T("vendor")});
}

// Do not resize if manifest size matches size in target metadata. When resuming
// from an update, do not redo the resize if not needed.
TEST_P(BootControlAndroidTestP, NoResizeIfSizeMatchWhenAdding) {
  SetMetadata(source(), {{S("system"), 2_GiB}, {T("system"), 2_GiB}});
  SetMetadata(
      target(),
      {{S("system"), 2_GiB}, {T("system"), 2_GiB}, {T("vendor"), 1_GiB}});
  // Should not need to resize and store metadata
  EXPECT_CALL(dynamicControl(), StoreMetadata(GetSuperDevice(), _, target()))
      .Times(0);
  ExpectRemap({T("system"), T("vendor")});

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(
      target(), {{"system", 2_GiB}, {"vendor", 1_GiB}}));
  ExpectDevicesAreMapped({T("system"), T("vendor")});
}

// Do not resize if manifest size matches size in target metadata. When resuming
// from an update, do not redo the resize if not needed.
TEST_P(BootControlAndroidTestP, NoResizeIfSizeMatchWhenDeleting) {
  SetMetadata(source(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 2_GiB},
               {T("vendor"), 1_GiB}});
  SetMetadata(target(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 2_GiB},
               {T("vendor"), 0}});
  // Should not need to resize and store metadata
  EXPECT_CALL(dynamicControl(), StoreMetadata(GetSuperDevice(), _, target()))
      .Times(0);
  ExpectUnmap({T("system"), T("vendor")});
  ExpectMap({T("system")});

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(
      target(), {{"system", 2_GiB}, {"vendor", 0}}));
  ExpectDevicesAreMapped({T("system")});
}

// Test resize case. Grow if target metadata contains a partition with a size
// less than expected.
TEST_P(BootControlAndroidTestP, NeedGrowIfSizeNotMatchWhenResizing) {
  PartitionSizes initial{{S("system"), 2_GiB},
                         {S("vendor"), 1_GiB},
                         {T("system"), 2_GiB},
                         {T("vendor"), 1_GiB}};
  SetMetadata(source(), initial);
  SetMetadata(target(), initial);
  EXPECT_CALL(dynamicControl(),
              StoreMetadata(GetSuperDevice(),
                            MetadataMatches({{S("system"), 2_GiB},
                                             {S("vendor"), 1_GiB},
                                             {T("system"), 3_GiB},
                                             {T("vendor"), 1_GiB}}),
                            target()))
      .WillOnce(Return(true));
  ExpectRemap({T("system"), T("vendor")});

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(
      target(), {{"system", 3_GiB}, {"vendor", 1_GiB}}));
  ExpectDevicesAreMapped({T("system"), T("vendor")});
}

// Test resize case. Shrink if target metadata contains a partition with a size
// greater than expected.
TEST_P(BootControlAndroidTestP, NeedShrinkIfSizeNotMatchWhenResizing) {
  PartitionSizes initial{{S("system"), 2_GiB},
                         {S("vendor"), 1_GiB},
                         {T("system"), 2_GiB},
                         {T("vendor"), 1_GiB}};
  SetMetadata(source(), initial);
  SetMetadata(target(), initial);
  EXPECT_CALL(dynamicControl(),
              StoreMetadata(GetSuperDevice(),
                            MetadataMatches({{S("system"), 2_GiB},
                                             {S("vendor"), 1_GiB},
                                             {T("system"), 2_GiB},
                                             {T("vendor"), 150_MiB}}),
                            target()))
      .WillOnce(Return(true));
  ExpectRemap({T("system"), T("vendor")});

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(
      target(), {{"system", 2_GiB}, {"vendor", 150_MiB}}));
  ExpectDevicesAreMapped({T("system"), T("vendor")});
}

// Test adding partitions on the first run.
TEST_P(BootControlAndroidTestP, AddPartitionToEmptyMetadata) {
  SetMetadata(source(), {});
  SetMetadata(target(), {});
  EXPECT_CALL(dynamicControl(),
              StoreMetadata(
                  GetSuperDevice(),
                  MetadataMatches({{T("system"), 2_GiB}, {T("vendor"), 1_GiB}}),
                  target()))
      .WillOnce(Return(true));
  ExpectRemap({T("system"), T("vendor")});

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(
      target(), {{"system", 2_GiB}, {"vendor", 1_GiB}}));
  ExpectDevicesAreMapped({T("system"), T("vendor")});
}

// Test subsequent add case.
TEST_P(BootControlAndroidTestP, AddAdditionalPartition) {
  SetMetadata(source(), {{S("system"), 2_GiB}, {T("system"), 2_GiB}});
  SetMetadata(target(), {{S("system"), 2_GiB}, {T("system"), 2_GiB}});
  EXPECT_CALL(dynamicControl(),
              StoreMetadata(GetSuperDevice(),
                            MetadataMatches({{S("system"), 2_GiB},
                                             {T("system"), 2_GiB},
                                             {T("vendor"), 1_GiB}}),
                            target()))
      .WillOnce(Return(true));
  ExpectRemap({T("system"), T("vendor")});

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(
      target(), {{"system", 2_GiB}, {"vendor", 1_GiB}}));
  ExpectDevicesAreMapped({T("system"), T("vendor")});
}

// Test delete one partition.
TEST_P(BootControlAndroidTestP, DeletePartition) {
  PartitionSizes initial{{S("system"), 2_GiB},
                         {S("vendor"), 1_GiB},
                         {T("system"), 2_GiB},
                         {T("vendor"), 1_GiB}};
  SetMetadata(source(), initial);
  SetMetadata(target(), initial);
  EXPECT_CALL(dynamicControl(),
              StoreMetadata(GetSuperDevice(),
                            MetadataMatches({{S("system"), 2_GiB},
                                             {S("vendor"), 1_GiB},
                                             {T("system"), 2_GiB},
                                             {T("vendor"), 0}}),
                            target()))
      .WillOnce(Return(true));
  ExpectUnmap({T("system"), T("vendor")});
  ExpectMap({T("system")});

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(
      target(), {{"system", 2_GiB}, {"vendor", 0}}));
  ExpectDevicesAreMapped({T("system")});
}

// Test delete all partitions.
TEST_P(BootControlAndroidTestP, DeleteAll) {
  PartitionSizes initial{{S("system"), 2_GiB},
                         {S("vendor"), 1_GiB},
                         {T("system"), 2_GiB},
                         {T("vendor"), 1_GiB}};
  SetMetadata(source(), initial);
  SetMetadata(target(), initial);
  EXPECT_CALL(dynamicControl(),
              StoreMetadata(GetSuperDevice(),
                            MetadataMatches({{S("system"), 2_GiB},
                                             {S("vendor"), 1_GiB},
                                             {T("system"), 0},
                                             {T("vendor"), 0}}),
                            target()))
      .WillOnce(Return(true));
  ExpectUnmap({T("system"), T("vendor")});
  ExpectMap({});

  EXPECT_TRUE(
      bootctl_.InitPartitionMetadata(target(), {{"system", 0}, {"vendor", 0}}));
  ExpectDevicesAreMapped({});
}

// Test corrupt source metadata case. This shouldn't happen in practice,
// because the device is already booted normally.
TEST_P(BootControlAndroidTestP, CorruptedSourceMetadata) {
  EXPECT_CALL(dynamicControl(), LoadMetadataBuilder(GetSuperDevice(), source()))
      .WillOnce(Invoke([](auto, auto) { return nullptr; }));
  EXPECT_FALSE(bootctl_.InitPartitionMetadata(target(), {}))
      << "Should not be able to continue with corrupt source metadata";
}

// Test corrupt target metadata case. This may happen in practice.
// BootControlAndroid should copy from source metadata and make necessary
// modifications on it.
TEST_P(BootControlAndroidTestP, CorruptedTargetMetadata) {
  SetMetadata(source(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 0},
               {T("vendor"), 0}});
  EXPECT_CALL(dynamicControl(), LoadMetadataBuilder(GetSuperDevice(), target()))
      .WillOnce(Invoke([](auto, auto) { return nullptr; }));
  EXPECT_CALL(dynamicControl(),
              StoreMetadata(GetSuperDevice(),
                            MetadataMatches({{S("system"), 2_GiB},
                                             {S("vendor"), 1_GiB},
                                             {T("system"), 3_GiB},
                                             {T("vendor"), 150_MiB}}),
                            target()))
      .WillOnce(Return(true));
  ExpectRemap({T("system"), T("vendor")});
  EXPECT_TRUE(bootctl_.InitPartitionMetadata(
      target(), {{"system", 3_GiB}, {"vendor", 150_MiB}}));
  ExpectDevicesAreMapped({T("system"), T("vendor")});
}

// Test that InitPartitionMetadata fail if there is not enough space on the
// device.
TEST_P(BootControlAndroidTestP, NotEnoughSpace) {
  PartitionSizes initial{{S("system"), 3_GiB},
                         {S("vendor"), 2_GiB},
                         {T("system"), 0},
                         {T("vendor"), 0}};
  SetMetadata(source(), initial);
  SetMetadata(target(), initial);
  EXPECT_FALSE(bootctl_.InitPartitionMetadata(
      target(), {{"system", 3_GiB}, {"vendor", 3_GiB}}))
      << "Should not be able to fit 11GiB data into 10GiB space";
}

INSTANTIATE_TEST_CASE_P(ParamTest,
                        BootControlAndroidTestP,
                        testing::Values(TestParam{0, 1}, TestParam{1, 0}));

const PartitionSizes update_sizes_0() {
  return {{"grown_a", 2_GiB},
          {"shrunk_a", 1_GiB},
          {"same_a", 100_MiB},
          {"deleted_a", 150_MiB},
          {"grown_b", 200_MiB},
          {"shrunk_b", 0},
          {"same_b", 0}};
}

const PartitionSizes update_sizes_1() {
  return {
      {"grown_a", 2_GiB},
      {"shrunk_a", 1_GiB},
      {"same_a", 100_MiB},
      {"deleted_a", 150_MiB},
      {"grown_b", 3_GiB},
      {"shrunk_b", 150_MiB},
      {"same_b", 100_MiB},
      {"added_b", 150_MiB},
      {"deleted_b", 0},
  };
}

const PartitionSizes update_sizes_2() {
  return {{"grown_a", 4_GiB},
          {"shrunk_a", 100_MiB},
          {"same_a", 100_MiB},
          {"added_a", 0_MiB},
          {"deleted_a", 64_MiB},
          {"grown_b", 3_GiB},
          {"shrunk_b", 150_MiB},
          {"same_b", 100_MiB},
          {"added_b", 150_MiB},
          {"deleted_b", 0}};
}

// Test case for first update after the device is manufactured, in which
// case the "other" slot is likely of size "0" (except system, which is
// non-zero because of system_other partition)
TEST_F(BootControlAndroidTest, SimulatedFirstUpdate) {
  SetSlots({0, 1});

  SetMetadata(source(), update_sizes_0());
  SetMetadata(target(), update_sizes_0());
  EXPECT_CALL(
      dynamicControl(),
      StoreMetadata(
          GetSuperDevice(), MetadataMatches(update_sizes_1()), target()))
      .WillOnce(Return(true));
  ExpectUnmap({"grown_b", "shrunk_b", "same_b", "added_b", "deleted_b"});
  ExpectMap({"grown_b", "shrunk_b", "same_b", "added_b"});

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(target(),
                                             {{"grown", 3_GiB},
                                              {"shrunk", 150_MiB},
                                              {"same", 100_MiB},
                                              {"added", 150_MiB},
                                              {"deleted", 0_MiB}}));
  ExpectDevicesAreMapped({"grown_b", "shrunk_b", "same_b", "added_b"});
}

// After first update, test for the second update. In the second update, the
// "added" partition is deleted and "deleted" partition is re-added.
TEST_F(BootControlAndroidTest, SimulatedSecondUpdate) {
  SetSlots({1, 0});

  SetMetadata(source(), update_sizes_1());
  SetMetadata(target(), update_sizes_0());

  EXPECT_CALL(
      dynamicControl(),
      StoreMetadata(
          GetSuperDevice(), MetadataMatches(update_sizes_2()), target()))
      .WillOnce(Return(true));
  ExpectUnmap({"grown_a", "shrunk_a", "same_a", "added_a", "deleted_a"});
  ExpectMap({"grown_a", "shrunk_a", "same_a", "deleted_a"});

  EXPECT_TRUE(bootctl_.InitPartitionMetadata(target(),
                                             {{"grown", 4_GiB},
                                              {"shrunk", 100_MiB},
                                              {"same", 100_MiB},
                                              {"added", 0_MiB},
                                              {"deleted", 64_MiB}}));
  ExpectDevicesAreMapped({"grown_a", "shrunk_a", "same_a", "deleted_a"});
}

TEST_F(BootControlAndroidTest, ApplyingToCurrentSlot) {
  SetSlots({1, 1});
  EXPECT_FALSE(bootctl_.InitPartitionMetadata(target(), {}))
      << "Should not be able to apply to current slot.";
}

}  // namespace chromeos_update_engine
