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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "update_engine/common/test_utils.h"
#include "update_engine/payload_consumer/payload_constants.h"
#include "update_engine/payload_generator/extent_utils.h"
#include "update_engine/payload_generator/merge_sequence_generator.h"

using chromeos_update_engine::test_utils::FillWithData;
using std::string;
using std::vector;

namespace chromeos_update_engine {
class MergeSequenceGeneratorTest : public ::testing::Test {
 protected:
  void VerifyTransfers(MergeSequenceGenerator* generator,
                       const std::vector<CowMergeOperation>& expected) {
    ASSERT_EQ(expected, generator->operations_);
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

}  // namespace chromeos_update_engine
