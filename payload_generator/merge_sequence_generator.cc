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

#include "update_engine/payload_generator/merge_sequence_generator.h"

#include "update_engine/payload_generator/extent_utils.h"

namespace chromeos_update_engine {

CowMergeOperation CreateCowMergeOperation(const Extent& src_extent,
                                          const Extent& dst_extent) {
  CowMergeOperation ret;
  ret.set_type(CowMergeOperation::COW_COPY);
  *ret.mutable_src_extent() = src_extent;
  *ret.mutable_dst_extent() = dst_extent;
  return ret;
}

std::ostream& operator<<(std::ostream& os,
                         const CowMergeOperation& merge_operation) {
  os << "CowMergeOperation src extent: "
     << ExtentsToString({merge_operation.src_extent()})
     << ", dst extent: " << ExtentsToString({merge_operation.dst_extent()});
  return os;
}

// The OTA generation guarantees that all blocks in the dst extent will be
// written only once. So we can use it to order the CowMergeOperation.
bool operator<(const CowMergeOperation& op1, const CowMergeOperation& op2) {
  return op1.dst_extent().start_block() < op2.dst_extent().start_block();
}

bool operator==(const CowMergeOperation& op1, const CowMergeOperation& op2) {
  return op1.type() == op2.type() && op1.src_extent() == op2.src_extent() &&
         op1.dst_extent() == op2.dst_extent();
}

std::unique_ptr<MergeSequenceGenerator> MergeSequenceGenerator::Create(
    const std::vector<AnnotatedOperation>& aops) {
  std::vector<CowMergeOperation> sequence;
  for (const auto& aop : aops) {
    // Only handle SOURCE_COPY now for the cow size optimization.
    if (aop.op.type() != InstallOperation::SOURCE_COPY) {
      continue;
    }
    if (aop.op.dst_extents().size() != 1) {
      std::vector<Extent> out_extents;
      ExtentsToVector(aop.op.dst_extents(), &out_extents);
      LOG(ERROR) << "The dst extents for source_copy expects to be contiguous,"
                 << " dst extents: " << ExtentsToString(out_extents);
      return nullptr;
    }

    // Split the source extents.
    size_t used_blocks = 0;
    for (const auto& src_extent : aop.op.src_extents()) {
      // The dst_extent in the merge sequence will be a subset of
      // InstallOperation's dst_extent. This will simplify the OTA -> COW
      // conversion when we install the payload.
      Extent dst_extent =
          ExtentForRange(aop.op.dst_extents(0).start_block() + used_blocks,
                         src_extent.num_blocks());
      sequence.emplace_back(CreateCowMergeOperation(src_extent, dst_extent));
      used_blocks += src_extent.num_blocks();
    }

    if (used_blocks != aop.op.dst_extents(0).num_blocks()) {
      LOG(ERROR) << "Number of blocks in src extents doesn't equal to the"
                 << " ones in the dst extents, src blocks " << used_blocks
                 << ", dst blocks " << aop.op.dst_extents(0).num_blocks();
      return nullptr;
    }
  }

  return std::unique_ptr<MergeSequenceGenerator>(
      new MergeSequenceGenerator(sequence));
}

bool MergeSequenceGenerator::FindDependency(
    std::map<CowMergeOperation, std::set<CowMergeOperation>>* result) const {
  CHECK(result);
  return true;
}

bool MergeSequenceGenerator::Generate(
    std::vector<CowMergeOperation>* sequence) const {
  return true;
}

bool MergeSequenceGenerator::ValidateSequence(
    const std::vector<CowMergeOperation>& sequence) {
  LOG(INFO) << "Validating merge sequence";
  ExtentRanges visited;
  for (const auto& op : sequence) {
    if (visited.OverlapsWithExtent(op.src_extent())) {
      LOG(ERROR) << "Transfer violates the merge sequence " << op
                 << "Visited extent ranges: ";
      visited.Dump();
      return false;
    }

    CHECK(!visited.OverlapsWithExtent(op.dst_extent()))
        << "dst extent should write only once.";
    visited.AddExtent(op.dst_extent());
  }

  return true;
}

}  // namespace chromeos_update_engine
