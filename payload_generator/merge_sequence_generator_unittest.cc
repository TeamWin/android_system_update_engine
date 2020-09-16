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
#include <vector>

#include <gtest/gtest.h>

#include "update_engine/payload_consumer/payload_constants.h"
#include "update_engine/payload_generator/extent_utils.h"
#include "update_engine/payload_generator/merge_sequence_generator.h"

namespace chromeos_update_engine {
class MergeSequenceGeneratorTest : public ::testing::Test {
 protected:
  void VerifyTransfers(MergeSequenceGenerator* generator,
                       const std::vector<CowMergeOperation>& expected) {
    ASSERT_EQ(expected, generator->operations_);
  }

  void FindDependency(
      std::vector<CowMergeOperation> transfers,
      std::map<CowMergeOperation, std::set<CowMergeOperation>>* result) {
    std::sort(transfers.begin(), transfers.end());
    MergeSequenceGenerator generator(std::move(transfers));
    ASSERT_TRUE(generator.FindDependency(result));
  }

  void GenerateSequence(std::vector<CowMergeOperation> transfers,
                        const std::vector<CowMergeOperation>& expected) {
    std::sort(transfers.begin(), transfers.end());
    MergeSequenceGenerator generator(std::move(transfers));
    std::vector<CowMergeOperation> sequence;
    ASSERT_TRUE(generator.Generate(&sequence));
    ASSERT_EQ(expected, sequence);
  }
};

TEST_F(MergeSequenceGeneratorTest, Create) {
  std::vector<AnnotatedOperation> aops{{"file1", {}}, {"file2", {}}};
  aops[0].op.set_type(InstallOperation::SOURCE_COPY);
  *aops[0].op.add_src_extents() = ExtentForRange(10, 10);
  *aops[0].op.add_dst_extents() = ExtentForRange(30, 10);

  aops[1].op.set_type(InstallOperation::SOURCE_COPY);
  *aops[1].op.add_src_extents() = ExtentForRange(20, 10);
  *aops[1].op.add_dst_extents() = ExtentForRange(40, 10);

  auto generator = MergeSequenceGenerator::Create(aops);
  ASSERT_TRUE(generator);
  std::vector<CowMergeOperation> expected = {
      CreateCowMergeOperation(ExtentForRange(10, 10), ExtentForRange(30, 10)),
      CreateCowMergeOperation(ExtentForRange(20, 10), ExtentForRange(40, 10))};
  VerifyTransfers(generator.get(), expected);

  *aops[1].op.add_src_extents() = ExtentForRange(30, 5);
  *aops[1].op.add_dst_extents() = ExtentForRange(50, 5);
  generator = MergeSequenceGenerator::Create(aops);
  ASSERT_FALSE(generator);
}

TEST_F(MergeSequenceGeneratorTest, Create_SplitSource) {
  InstallOperation op;
  op.set_type(InstallOperation::SOURCE_COPY);
  *(op.add_src_extents()) = ExtentForRange(2, 3);
  *(op.add_src_extents()) = ExtentForRange(6, 1);
  *(op.add_src_extents()) = ExtentForRange(8, 4);
  *(op.add_dst_extents()) = ExtentForRange(10, 8);

  AnnotatedOperation aop{"file1", op};
  auto generator = MergeSequenceGenerator::Create({aop});
  ASSERT_TRUE(generator);
  std::vector<CowMergeOperation> expected = {
      CreateCowMergeOperation(ExtentForRange(2, 3), ExtentForRange(10, 3)),
      CreateCowMergeOperation(ExtentForRange(6, 1), ExtentForRange(13, 1)),
      CreateCowMergeOperation(ExtentForRange(8, 4), ExtentForRange(14, 4))};
  VerifyTransfers(generator.get(), expected);
}

TEST_F(MergeSequenceGeneratorTest, FindDependency) {
  std::vector<CowMergeOperation> transfers = {
      CreateCowMergeOperation(ExtentForRange(10, 10), ExtentForRange(15, 10)),
      CreateCowMergeOperation(ExtentForRange(40, 10), ExtentForRange(50, 10)),
  };

  std::map<CowMergeOperation, std::set<CowMergeOperation>> merge_after;
  FindDependency(transfers, &merge_after);
  ASSERT_EQ(std::set<CowMergeOperation>(), merge_after.at(transfers[0]));
  ASSERT_EQ(std::set<CowMergeOperation>(), merge_after.at(transfers[1]));

  transfers = {
      CreateCowMergeOperation(ExtentForRange(10, 10), ExtentForRange(25, 10)),
      CreateCowMergeOperation(ExtentForRange(24, 5), ExtentForRange(35, 5)),
      CreateCowMergeOperation(ExtentForRange(30, 10), ExtentForRange(15, 10)),
  };

  FindDependency(transfers, &merge_after);
  ASSERT_EQ(std::set<CowMergeOperation>({transfers[2]}),
            merge_after.at(transfers[0]));
  ASSERT_EQ(std::set<CowMergeOperation>({transfers[0], transfers[2]}),
            merge_after.at(transfers[1]));
  ASSERT_EQ(std::set<CowMergeOperation>({transfers[0], transfers[1]}),
            merge_after.at(transfers[2]));
}

TEST_F(MergeSequenceGeneratorTest, FindDependency_ReusedSourceBlocks) {
  std::vector<CowMergeOperation> transfers = {
      CreateCowMergeOperation(ExtentForRange(5, 10), ExtentForRange(15, 10)),
      CreateCowMergeOperation(ExtentForRange(6, 5), ExtentForRange(30, 5)),
      CreateCowMergeOperation(ExtentForRange(50, 5), ExtentForRange(5, 5)),
  };

  std::map<CowMergeOperation, std::set<CowMergeOperation>> merge_after;
  FindDependency(transfers, &merge_after);
  ASSERT_EQ(std::set<CowMergeOperation>({transfers[2]}),
            merge_after.at(transfers[0]));
  ASSERT_EQ(std::set<CowMergeOperation>({transfers[2]}),
            merge_after.at(transfers[1]));
}

TEST_F(MergeSequenceGeneratorTest, ValidateSequence) {
  std::vector<CowMergeOperation> transfers = {
      CreateCowMergeOperation(ExtentForRange(10, 10), ExtentForRange(15, 10)),
      CreateCowMergeOperation(ExtentForRange(30, 10), ExtentForRange(40, 10)),
  };

  // Self overlapping
  ASSERT_TRUE(MergeSequenceGenerator::ValidateSequence(transfers));

  transfers = {
      CreateCowMergeOperation(ExtentForRange(30, 10), ExtentForRange(20, 10)),
      CreateCowMergeOperation(ExtentForRange(15, 10), ExtentForRange(10, 10)),
  };
  ASSERT_FALSE(MergeSequenceGenerator::ValidateSequence(transfers));
}

TEST_F(MergeSequenceGeneratorTest, GenerateSequenceNoCycles) {
  std::vector<CowMergeOperation> transfers = {
      CreateCowMergeOperation(ExtentForRange(10, 10), ExtentForRange(15, 10)),
      // file3 should merge before file2
      CreateCowMergeOperation(ExtentForRange(40, 5), ExtentForRange(25, 5)),
      CreateCowMergeOperation(ExtentForRange(25, 10), ExtentForRange(30, 10)),
  };

  std::vector<CowMergeOperation> expected{
      transfers[0], transfers[2], transfers[1]};
  GenerateSequence(transfers, expected);
}

TEST_F(MergeSequenceGeneratorTest, GenerateSequenceWithCycles) {
  std::vector<CowMergeOperation> transfers = {
      CreateCowMergeOperation(ExtentForRange(25, 10), ExtentForRange(30, 10)),
      CreateCowMergeOperation(ExtentForRange(30, 10), ExtentForRange(40, 10)),
      CreateCowMergeOperation(ExtentForRange(40, 10), ExtentForRange(25, 10)),
      CreateCowMergeOperation(ExtentForRange(10, 10), ExtentForRange(15, 10)),
  };

  // file 1,2,3 form a cycle. And file3, whose dst ext has smallest offset, will
  // be converted to raw blocks
  std::vector<CowMergeOperation> expected{
      transfers[3], transfers[1], transfers[0]};
  GenerateSequence(transfers, expected);
}

TEST_F(MergeSequenceGeneratorTest, GenerateSequenceMultipleCycles) {
  std::vector<CowMergeOperation> transfers = {
      // cycle 1
      CreateCowMergeOperation(ExtentForRange(10, 10), ExtentForRange(25, 10)),
      CreateCowMergeOperation(ExtentForRange(24, 5), ExtentForRange(35, 5)),
      CreateCowMergeOperation(ExtentForRange(30, 10), ExtentForRange(15, 10)),
      // cycle 2
      CreateCowMergeOperation(ExtentForRange(55, 10), ExtentForRange(60, 10)),
      CreateCowMergeOperation(ExtentForRange(60, 10), ExtentForRange(70, 10)),
      CreateCowMergeOperation(ExtentForRange(70, 10), ExtentForRange(55, 10)),
  };

  // file 3, 6 will be converted to raw.
  std::vector<CowMergeOperation> expected{
      transfers[1], transfers[0], transfers[4], transfers[3]};
  GenerateSequence(transfers, expected);
}

}  // namespace chromeos_update_engine
