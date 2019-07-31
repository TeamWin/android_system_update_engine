//
// Copyright (C) 2019 The Android Open Source Project
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

#include "update_engine/dynamic_partition_control_android.h"

#include <set>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "update_engine/dynamic_partition_test_utils.h"
#include "update_engine/mock_dynamic_partition_control.h"

using std::string;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::NiceMock;
using testing::Not;
using testing::Return;

namespace chromeos_update_engine {

class DynamicPartitionControlAndroidTest : public ::testing::Test {
 public:
  void SetUp() override {
    module_ = std::make_unique<NiceMock<MockDynamicPartitionControlAndroid>>();

    ON_CALL(dynamicControl(), GetDynamicPartitionsFeatureFlag())
        .WillByDefault(Return(FeatureFlag(FeatureFlag::Value::LAUNCH)));

    ON_CALL(dynamicControl(), GetDeviceDir(_))
        .WillByDefault(Invoke([](auto path) {
          *path = kFakeDevicePath;
          return true;
        }));

    ON_CALL(dynamicControl(), GetSuperPartitionName(_))
        .WillByDefault(Return(kFakeSuper));
  }

  // Return the mocked DynamicPartitionControlInterface.
  NiceMock<MockDynamicPartitionControlAndroid>& dynamicControl() {
    return static_cast<NiceMock<MockDynamicPartitionControlAndroid>&>(*module_);
  }

  std::string GetSuperDevice(uint32_t slot) {
    return GetDevice(dynamicControl().GetSuperPartitionName(slot));
  }

  uint32_t source() { return slots_.source; }
  uint32_t target() { return slots_.target; }

  // Return partition names with suffix of source().
  std::string S(const std::string& name) {
    return name + kSlotSuffixes[source()];
  }

  // Return partition names with suffix of target().
  std::string T(const std::string& name) {
    return name + kSlotSuffixes[target()];
  }

  // Set the fake metadata to return when LoadMetadataBuilder is called on
  // |slot|.
  void SetMetadata(uint32_t slot, const PartitionSuffixSizes& sizes) {
    EXPECT_CALL(dynamicControl(),
                LoadMetadataBuilder(GetSuperDevice(slot), slot, _))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke([sizes](auto, auto, auto) {
          return NewFakeMetadata(PartitionSuffixSizesToMetadata(sizes));
        }));
  }

  void ExpectStoreMetadata(const PartitionSuffixSizes& partition_sizes) {
    EXPECT_CALL(dynamicControl(),
                StoreMetadata(GetSuperDevice(target()),
                              MetadataMatches(partition_sizes),
                              target()))
        .WillOnce(Return(true));
  }

  // Expect that UnmapPartitionOnDeviceMapper is called on target() metadata
  // slot with each partition in |partitions|.
  void ExpectUnmap(const std::set<std::string>& partitions) {
    // Error when UnmapPartitionOnDeviceMapper is called on unknown arguments.
    ON_CALL(dynamicControl(), UnmapPartitionOnDeviceMapper(_))
        .WillByDefault(Return(false));

    for (const auto& partition : partitions) {
      EXPECT_CALL(dynamicControl(), UnmapPartitionOnDeviceMapper(partition))
          .WillOnce(Return(true));
    }
  }
  bool PreparePartitionsForUpdate(const PartitionSizes& partition_sizes) {
    return dynamicControl().PreparePartitionsForUpdate(
        source(), target(), PartitionSizesToMetadata(partition_sizes));
  }
  void SetSlots(const TestParam& slots) { slots_ = slots; }

  struct Listener : public ::testing::MatchResultListener {
    explicit Listener(std::ostream* os) : MatchResultListener(os) {}
  };

  testing::AssertionResult UpdatePartitionMetadata(
      const PartitionSuffixSizes& source_metadata,
      const PartitionSizes& update_metadata,
      const PartitionSuffixSizes& expected) {
    return UpdatePartitionMetadata(
        PartitionSuffixSizesToMetadata(source_metadata),
        PartitionSizesToMetadata(update_metadata),
        PartitionSuffixSizesToMetadata(expected));
  }
  testing::AssertionResult UpdatePartitionMetadata(
      const PartitionMetadata& source_metadata,
      const PartitionMetadata& update_metadata,
      const PartitionMetadata& expected) {
    return UpdatePartitionMetadata(
        source_metadata, update_metadata, MetadataMatches(expected));
  }
  testing::AssertionResult UpdatePartitionMetadata(
      const PartitionMetadata& source_metadata,
      const PartitionMetadata& update_metadata,
      const Matcher<MetadataBuilder*>& matcher) {
    auto super_metadata = NewFakeMetadata(source_metadata);
    if (!module_->UpdatePartitionMetadata(
            super_metadata.get(), target(), update_metadata)) {
      return testing::AssertionFailure()
             << "UpdatePartitionMetadataInternal failed";
    }
    std::stringstream ss;
    Listener listener(&ss);
    if (matcher.MatchAndExplain(super_metadata.get(), &listener)) {
      return testing::AssertionSuccess() << ss.str();
    } else {
      return testing::AssertionFailure() << ss.str();
    }
  }

  std::unique_ptr<DynamicPartitionControlAndroid> module_;
  TestParam slots_;
};

class DynamicPartitionControlAndroidTestP
    : public DynamicPartitionControlAndroidTest,
      public ::testing::WithParamInterface<TestParam> {
 public:
  void SetUp() override {
    DynamicPartitionControlAndroidTest::SetUp();
    SetSlots(GetParam());
  }
};

// Test resize case. Grow if target metadata contains a partition with a size
// less than expected.
TEST_P(DynamicPartitionControlAndroidTestP,
       NeedGrowIfSizeNotMatchWhenResizing) {
  PartitionSuffixSizes source_metadata{{S("system"), 2_GiB},
                                       {S("vendor"), 1_GiB},
                                       {T("system"), 2_GiB},
                                       {T("vendor"), 1_GiB}};
  PartitionSuffixSizes expected{{S("system"), 2_GiB},
                                {S("vendor"), 1_GiB},
                                {T("system"), 3_GiB},
                                {T("vendor"), 1_GiB}};
  PartitionSizes update_metadata{{"system", 3_GiB}, {"vendor", 1_GiB}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test resize case. Shrink if target metadata contains a partition with a size
// greater than expected.
TEST_P(DynamicPartitionControlAndroidTestP,
       NeedShrinkIfSizeNotMatchWhenResizing) {
  PartitionSuffixSizes source_metadata{{S("system"), 2_GiB},
                                       {S("vendor"), 1_GiB},
                                       {T("system"), 2_GiB},
                                       {T("vendor"), 1_GiB}};
  PartitionSuffixSizes expected{{S("system"), 2_GiB},
                                {S("vendor"), 1_GiB},
                                {T("system"), 2_GiB},
                                {T("vendor"), 150_MiB}};
  PartitionSizes update_metadata{{"system", 2_GiB}, {"vendor", 150_MiB}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test adding partitions on the first run.
TEST_P(DynamicPartitionControlAndroidTestP, AddPartitionToEmptyMetadata) {
  PartitionSuffixSizes source_metadata{};
  PartitionSuffixSizes expected{{T("system"), 2_GiB}, {T("vendor"), 1_GiB}};
  PartitionSizes update_metadata{{"system", 2_GiB}, {"vendor", 1_GiB}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test subsequent add case.
TEST_P(DynamicPartitionControlAndroidTestP, AddAdditionalPartition) {
  PartitionSuffixSizes source_metadata{{S("system"), 2_GiB},
                                       {T("system"), 2_GiB}};
  PartitionSuffixSizes expected{
      {S("system"), 2_GiB}, {T("system"), 2_GiB}, {T("vendor"), 1_GiB}};
  PartitionSizes update_metadata{{"system", 2_GiB}, {"vendor", 1_GiB}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test delete one partition.
TEST_P(DynamicPartitionControlAndroidTestP, DeletePartition) {
  PartitionSuffixSizes source_metadata{{S("system"), 2_GiB},
                                       {S("vendor"), 1_GiB},
                                       {T("system"), 2_GiB},
                                       {T("vendor"), 1_GiB}};
  // No T("vendor")
  PartitionSuffixSizes expected{
      {S("system"), 2_GiB}, {S("vendor"), 1_GiB}, {T("system"), 2_GiB}};
  PartitionSizes update_metadata{{"system", 2_GiB}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test delete all partitions.
TEST_P(DynamicPartitionControlAndroidTestP, DeleteAll) {
  PartitionSuffixSizes source_metadata{{S("system"), 2_GiB},
                                       {S("vendor"), 1_GiB},
                                       {T("system"), 2_GiB},
                                       {T("vendor"), 1_GiB}};
  PartitionSuffixSizes expected{{S("system"), 2_GiB}, {S("vendor"), 1_GiB}};
  PartitionSizes update_metadata{};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test corrupt source metadata case.
TEST_P(DynamicPartitionControlAndroidTestP, CorruptedSourceMetadata) {
  EXPECT_CALL(dynamicControl(),
              LoadMetadataBuilder(GetSuperDevice(source()), source(), _))
      .WillOnce(Invoke([](auto, auto, auto) { return nullptr; }));
  ExpectUnmap({T("system")});

  EXPECT_FALSE(PreparePartitionsForUpdate({{"system", 1_GiB}}))
      << "Should not be able to continue with corrupt source metadata";
}

// Test that UpdatePartitionMetadata fails if there is not enough space on the
// device.
TEST_P(DynamicPartitionControlAndroidTestP, NotEnoughSpace) {
  PartitionSuffixSizes source_metadata{{S("system"), 3_GiB},
                                       {S("vendor"), 2_GiB},
                                       {T("system"), 0},
                                       {T("vendor"), 0}};
  PartitionSizes update_metadata{{"system", 3_GiB}, {"vendor", 3_GiB}};

  EXPECT_FALSE(UpdatePartitionMetadata(source_metadata, update_metadata, {}))
      << "Should not be able to fit 11GiB data into 10GiB space";
}

TEST_P(DynamicPartitionControlAndroidTestP, NotEnoughSpaceForSlot) {
  PartitionSuffixSizes source_metadata{{S("system"), 1_GiB},
                                       {S("vendor"), 1_GiB},
                                       {T("system"), 0},
                                       {T("vendor"), 0}};
  PartitionSizes update_metadata{{"system", 3_GiB}, {"vendor", 3_GiB}};
  EXPECT_FALSE(UpdatePartitionMetadata(source_metadata, update_metadata, {}))
      << "Should not be able to grow over size of super / 2";
}

INSTANTIATE_TEST_CASE_P(DynamicPartitionControlAndroidTest,
                        DynamicPartitionControlAndroidTestP,
                        testing::Values(TestParam{0, 1}, TestParam{1, 0}));

class DynamicPartitionControlAndroidGroupTestP
    : public DynamicPartitionControlAndroidTestP {
 public:
  PartitionMetadata source_metadata;
  void SetUp() override {
    DynamicPartitionControlAndroidTestP::SetUp();
    source_metadata = {
        .groups = {SimpleGroup(S("android"), 3_GiB, S("system"), 2_GiB),
                   SimpleGroup(S("oem"), 2_GiB, S("vendor"), 1_GiB),
                   SimpleGroup(T("android"), 3_GiB, T("system"), 0),
                   SimpleGroup(T("oem"), 2_GiB, T("vendor"), 0)}};
  }

  // Return a simple group with only one partition.
  PartitionMetadata::Group SimpleGroup(const string& group,
                                       uint64_t group_size,
                                       const string& partition,
                                       uint64_t partition_size) {
    return {.name = group,
            .size = group_size,
            .partitions = {{.name = partition, .size = partition_size}}};
  }
};

// Allow to resize within group.
TEST_P(DynamicPartitionControlAndroidGroupTestP, ResizeWithinGroup) {
  PartitionMetadata expected{
      .groups = {SimpleGroup(T("android"), 3_GiB, T("system"), 3_GiB),
                 SimpleGroup(T("oem"), 2_GiB, T("vendor"), 2_GiB)}};

  PartitionMetadata update_metadata{
      .groups = {SimpleGroup("android", 3_GiB, "system", 3_GiB),
                 SimpleGroup("oem", 2_GiB, "vendor", 2_GiB)}};

  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, NotEnoughSpaceForGroup) {
  PartitionMetadata update_metadata{
      .groups = {SimpleGroup("android", 3_GiB, "system", 1_GiB),
                 SimpleGroup("oem", 2_GiB, "vendor", 3_GiB)}};
  EXPECT_FALSE(UpdatePartitionMetadata(source_metadata, update_metadata, {}))
      << "Should not be able to grow over maximum size of group";
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, GroupTooBig) {
  PartitionMetadata update_metadata{
      .groups = {{.name = "android", .size = 3_GiB},
                 {.name = "oem", .size = 3_GiB}}};
  EXPECT_FALSE(UpdatePartitionMetadata(source_metadata, update_metadata, {}))
      << "Should not be able to grow over size of super / 2";
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, AddPartitionToGroup) {
  PartitionMetadata expected{
      .groups = {{.name = T("android"),
                  .size = 3_GiB,
                  .partitions = {{.name = T("system"), .size = 2_GiB},
                                 {.name = T("system_ext"), .size = 1_GiB}}}}};
  PartitionMetadata update_metadata{
      .groups = {{.name = "android",
                  .size = 3_GiB,
                  .partitions = {{.name = "system", .size = 2_GiB},
                                 {.name = "system_ext", .size = 1_GiB}}},
                 SimpleGroup("oem", 2_GiB, "vendor", 2_GiB)}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, RemovePartitionFromGroup) {
  PartitionMetadata expected{
      .groups = {{.name = T("android"), .size = 3_GiB, .partitions = {}}}};
  PartitionMetadata update_metadata{
      .groups = {{.name = "android", .size = 3_GiB, .partitions = {}},
                 SimpleGroup("oem", 2_GiB, "vendor", 2_GiB)}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, AddGroup) {
  PartitionMetadata expected{
      .groups = {
          SimpleGroup(T("new_group"), 2_GiB, T("new_partition"), 2_GiB)}};
  PartitionMetadata update_metadata{
      .groups = {SimpleGroup("android", 2_GiB, "system", 2_GiB),
                 SimpleGroup("oem", 1_GiB, "vendor", 1_GiB),
                 SimpleGroup("new_group", 2_GiB, "new_partition", 2_GiB)}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, RemoveGroup) {
  PartitionMetadata update_metadata{
      .groups = {SimpleGroup("android", 2_GiB, "system", 2_GiB)}};

  EXPECT_TRUE(UpdatePartitionMetadata(
      source_metadata, update_metadata, Not(HasGroup(T("oem")))));
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, ResizeGroup) {
  PartitionMetadata expected{
      .groups = {SimpleGroup(T("android"), 2_GiB, T("system"), 2_GiB),
                 SimpleGroup(T("oem"), 3_GiB, T("vendor"), 3_GiB)}};
  PartitionMetadata update_metadata{
      .groups = {SimpleGroup("android", 2_GiB, "system", 2_GiB),
                 SimpleGroup("oem", 3_GiB, "vendor", 3_GiB)}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

INSTANTIATE_TEST_CASE_P(DynamicPartitionControlAndroidTest,
                        DynamicPartitionControlAndroidGroupTestP,
                        testing::Values(TestParam{0, 1}, TestParam{1, 0}));

const PartitionSuffixSizes update_sizes_0() {
  // Initial state is 0 for "other" slot.
  return {
      {"grown_a", 2_GiB},
      {"shrunk_a", 1_GiB},
      {"same_a", 100_MiB},
      {"deleted_a", 150_MiB},
      // no added_a
      {"grown_b", 200_MiB},
      // simulate system_other
      {"shrunk_b", 0},
      {"same_b", 0},
      {"deleted_b", 0},
      // no added_b
  };
}

const PartitionSuffixSizes update_sizes_1() {
  return {
      {"grown_a", 2_GiB},
      {"shrunk_a", 1_GiB},
      {"same_a", 100_MiB},
      {"deleted_a", 150_MiB},
      // no added_a
      {"grown_b", 3_GiB},
      {"shrunk_b", 150_MiB},
      {"same_b", 100_MiB},
      {"added_b", 150_MiB},
      // no deleted_b
  };
}

const PartitionSuffixSizes update_sizes_2() {
  return {
      {"grown_a", 4_GiB},
      {"shrunk_a", 100_MiB},
      {"same_a", 100_MiB},
      {"deleted_a", 64_MiB},
      // no added_a
      {"grown_b", 3_GiB},
      {"shrunk_b", 150_MiB},
      {"same_b", 100_MiB},
      {"added_b", 150_MiB},
      // no deleted_b
  };
}

// Test case for first update after the device is manufactured, in which
// case the "other" slot is likely of size "0" (except system, which is
// non-zero because of system_other partition)
TEST_F(DynamicPartitionControlAndroidTest, SimulatedFirstUpdate) {
  SetSlots({0, 1});

  SetMetadata(source(), update_sizes_0());
  SetMetadata(target(), update_sizes_0());
  ExpectStoreMetadata(update_sizes_1());
  ExpectUnmap({"grown_b", "shrunk_b", "same_b", "added_b"});

  EXPECT_TRUE(PreparePartitionsForUpdate({{"grown", 3_GiB},
                                          {"shrunk", 150_MiB},
                                          {"same", 100_MiB},
                                          {"added", 150_MiB}}));
}

// After first update, test for the second update. In the second update, the
// "added" partition is deleted and "deleted" partition is re-added.
TEST_F(DynamicPartitionControlAndroidTest, SimulatedSecondUpdate) {
  SetSlots({1, 0});

  SetMetadata(source(), update_sizes_1());
  SetMetadata(target(), update_sizes_0());

  ExpectStoreMetadata(update_sizes_2());
  ExpectUnmap({"grown_a", "shrunk_a", "same_a", "deleted_a"});

  EXPECT_TRUE(PreparePartitionsForUpdate({{"grown", 4_GiB},
                                          {"shrunk", 100_MiB},
                                          {"same", 100_MiB},
                                          {"deleted", 64_MiB}}));
}

}  // namespace chromeos_update_engine
