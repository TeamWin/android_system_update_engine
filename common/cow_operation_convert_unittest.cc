//
// Copyright (C) 2020 The Android Open Source Project
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
#include <array>
#include <initializer_list>

#include <gtest/gtest.h>

#include "update_engine/common/cow_operation_convert.h"
#include "update_engine/payload_generator/extent_ranges.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {
using OperationList = ::google::protobuf::RepeatedPtrField<
    ::chromeos_update_engine::InstallOperation>;
using MergeOplist = ::google::protobuf::RepeatedPtrField<
    ::chromeos_update_engine::CowMergeOperation>;

std::ostream& operator<<(std::ostream& out, CowOperation::Type op) {
  switch (op) {
    case CowOperation::Type::CowCopy:
      out << "CowCopy";
      break;
    case CowOperation::Type::CowReplace:
      out << "CowReplace";
      break;
    default:
      out << op;
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const CowOperation& c) {
  out << "{" << c.op << ", " << c.src_block << ", " << c.dst_block << "}";
  return out;
}

class CowOperationConvertTest : public testing::Test {
 public:
  void VerifyCowMergeOp(const std::vector<CowOperation>& cow_ops) {
    // Build a set of all extents covered by InstallOps.
    ExtentRanges src_extent_set;
    ExtentRanges dst_extent_set;
    for (auto&& op : operations_) {
      src_extent_set.AddRepeatedExtents(op.src_extents());
      dst_extent_set.AddRepeatedExtents(op.dst_extents());
    }
    ExtentRanges modified_extents;
    for (auto&& cow_op : cow_ops) {
      if (cow_op.op == CowOperation::CowCopy) {
        EXPECT_TRUE(src_extent_set.ContainsBlock(cow_op.src_block));
        // converted operations should be conflict free.
        EXPECT_FALSE(modified_extents.ContainsBlock(cow_op.src_block))
            << "SOURCE_COPY operation " << cow_op
            << " read from a modified block";
      }
      EXPECT_TRUE(dst_extent_set.ContainsBlock(cow_op.dst_block));
      dst_extent_set.SubtractExtent(ExtentForRange(cow_op.dst_block, 1));
      modified_extents.AddBlock(cow_op.dst_block);
    }
    // The generated CowOps should cover all extents in InstallOps.
    EXPECT_EQ(dst_extent_set.blocks(), 0UL);
    // It's possible that src_extent_set is non-empty, because some operations
    // will be converted to CowReplace, and we don't count the source extent for
    // those.
  }
  OperationList operations_;
  MergeOplist merge_operations_;
};

void AddOperation(OperationList* operations,
                  ::chromeos_update_engine::InstallOperation_Type op_type,
                  std::initializer_list<std::array<int, 2>> src_extents,
                  std::initializer_list<std::array<int, 2>> dst_extents) {
  auto&& op = operations->Add();
  op->set_type(op_type);
  for (const auto& extent : src_extents) {
    *op->add_src_extents() = ExtentForRange(extent[0], extent[1]);
  }
  for (const auto& extent : dst_extents) {
    *op->add_dst_extents() = ExtentForRange(extent[0], extent[1]);
  }
}

void AddMergeOperation(MergeOplist* operations,
                       ::chromeos_update_engine::CowMergeOperation_Type op_type,
                       std::array<int, 2> src_extent,
                       std::array<int, 2> dst_extent) {
  auto&& op = operations->Add();
  op->set_type(op_type);
  *op->mutable_src_extent() = ExtentForRange(src_extent[0], src_extent[1]);
  *op->mutable_dst_extent() = ExtentForRange(dst_extent[0], dst_extent[1]);
}

TEST_F(CowOperationConvertTest, NoConflict) {
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{20, 1}}, {{30, 1}});
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{10, 1}}, {{20, 1}});
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{0, 1}}, {{10, 1}});

  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {20, 1}, {30, 1});
  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {10, 1}, {20, 1});
  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {0, 1}, {10, 1});

  auto cow_ops = ConvertToCowOperations(operations_, merge_operations_);
  ASSERT_EQ(cow_ops.size(), 3UL);
  ASSERT_TRUE(std::all_of(cow_ops.begin(), cow_ops.end(), [](auto&& cow_op) {
    return cow_op.op == CowOperation::CowCopy;
  }));
  VerifyCowMergeOp(cow_ops);
}

TEST_F(CowOperationConvertTest, CowReplace) {
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{30, 1}}, {{0, 1}});
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{20, 1}}, {{30, 1}});
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{10, 1}}, {{20, 1}});
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{0, 1}}, {{10, 1}});

  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {20, 1}, {30, 1});
  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {10, 1}, {20, 1});
  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {0, 1}, {10, 1});

  auto cow_ops = ConvertToCowOperations(operations_, merge_operations_);
  ASSERT_EQ(cow_ops.size(), 4UL);
  // Expect 3 COW_COPY and 1 COW_REPLACE
  ASSERT_EQ(std::count_if(cow_ops.begin(),
                          cow_ops.end(),
                          [](auto&& cow_op) {
                            return cow_op.op == CowOperation::CowCopy;
                          }),
            3);
  ASSERT_EQ(std::count_if(cow_ops.begin(),
                          cow_ops.end(),
                          [](auto&& cow_op) {
                            return cow_op.op == CowOperation::CowReplace;
                          }),
            1);
  VerifyCowMergeOp(cow_ops);
}

TEST_F(CowOperationConvertTest, ReOrderSourceCopy) {
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{30, 1}}, {{20, 1}});
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{20, 1}}, {{10, 1}});
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{10, 1}}, {{0, 1}});

  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {10, 1}, {0, 1});
  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {20, 1}, {10, 1});
  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {30, 1}, {20, 1});

  auto cow_ops = ConvertToCowOperations(operations_, merge_operations_);
  ASSERT_EQ(cow_ops.size(), 3UL);
  // Expect 3 COW_COPY
  ASSERT_TRUE(std::all_of(cow_ops.begin(), cow_ops.end(), [](auto&& cow_op) {
    return cow_op.op == CowOperation::CowCopy;
  }));
  VerifyCowMergeOp(cow_ops);
}

TEST_F(CowOperationConvertTest, InterleavingSrcExtent) {
  AddOperation(&operations_,
               InstallOperation::SOURCE_COPY,
               {{30, 5}, {35, 5}},
               {{20, 10}});
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{20, 1}}, {{10, 1}});
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{10, 1}}, {{0, 1}});

  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {10, 1}, {0, 1});
  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {20, 1}, {10, 1});
  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {30, 5}, {20, 5});
  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {35, 5}, {25, 5});

  auto cow_ops = ConvertToCowOperations(operations_, merge_operations_);
  // Expect 4 COW_COPY
  ASSERT_EQ(cow_ops.size(), 12UL);
  ASSERT_TRUE(std::all_of(cow_ops.begin(), cow_ops.end(), [](auto&& cow_op) {
    return cow_op.op == CowOperation::CowCopy;
  }));
  VerifyCowMergeOp(cow_ops);
}

TEST_F(CowOperationConvertTest, SelfOverlappingOperation) {
  AddOperation(
      &operations_, InstallOperation::SOURCE_COPY, {{20, 10}}, {{25, 10}});

  AddMergeOperation(
      &merge_operations_, CowMergeOperation::COW_COPY, {20, 10}, {25, 10});

  auto cow_ops = ConvertToCowOperations(operations_, merge_operations_);
  // Expect 10 COW_COPY
  ASSERT_EQ(cow_ops.size(), 10UL);
  ASSERT_TRUE(std::all_of(cow_ops.begin(), cow_ops.end(), [](auto&& cow_op) {
    return cow_op.op == CowOperation::CowCopy;
  }));
  VerifyCowMergeOp(cow_ops);
}

}  // namespace chromeos_update_engine
