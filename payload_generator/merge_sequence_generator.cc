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

#include <algorithm>

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

template <typename T>
constexpr T GetDifference(T first, T second) {
  T abs_diff = (first > second) ? (first - second) : (second - first);
  return abs_diff;
}

void SplitSelfOverlapping(const Extent& src_extent,
                          const Extent& dst_extent,
                          std::vector<CowMergeOperation>* sequence) {
  CHECK_EQ(src_extent.num_blocks(), dst_extent.num_blocks());
  if (src_extent.start_block() == dst_extent.start_block()) {
    sequence->emplace_back(CreateCowMergeOperation(src_extent, dst_extent));
    return;
  }

  const size_t diff =
      GetDifference(src_extent.start_block(), dst_extent.start_block());
  for (size_t i = 0; i < src_extent.num_blocks(); i += diff) {
    auto num_blocks = std::min<size_t>(diff, src_extent.num_blocks() - i);
    sequence->emplace_back(CreateCowMergeOperation(
        ExtentForRange(i + src_extent.start_block(), num_blocks),
        ExtentForRange(i + dst_extent.start_block(), num_blocks)));
  }
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

      // Self-overlapping SOURCE_COPY, must split into multiple non
      // self-overlapping ops
      if (ExtentRanges::ExtentsOverlap(src_extent, dst_extent)) {
        SplitSelfOverlapping(src_extent, dst_extent, &sequence);
      } else {
        sequence.emplace_back(CreateCowMergeOperation(src_extent, dst_extent));
      }
      used_blocks += src_extent.num_blocks();
    }

    if (used_blocks != aop.op.dst_extents(0).num_blocks()) {
      LOG(ERROR) << "Number of blocks in src extents doesn't equal to the"
                 << " ones in the dst extents, src blocks " << used_blocks
                 << ", dst blocks " << aop.op.dst_extents(0).num_blocks();
      return nullptr;
    }
  }

  std::sort(sequence.begin(), sequence.end());
  return std::unique_ptr<MergeSequenceGenerator>(
      new MergeSequenceGenerator(sequence));
}

bool MergeSequenceGenerator::FindDependency(
    std::map<CowMergeOperation, std::set<CowMergeOperation>>* result) const {
  CHECK(result);
  LOG(INFO) << "Finding dependencies";

  // Since the OTA operation may reuse some source blocks, use the binary
  // search on sorted dst extents to find overlaps.
  std::map<CowMergeOperation, std::set<CowMergeOperation>> merge_after;
  for (const auto& op : operations_) {
    // lower bound (inclusive): dst extent's end block >= src extent's start
    // block.
    const auto lower_it = std::lower_bound(
        operations_.begin(),
        operations_.end(),
        op,
        [](const CowMergeOperation& it, const CowMergeOperation& op) {
          auto dst_end_block =
              it.dst_extent().start_block() + it.dst_extent().num_blocks() - 1;
          return dst_end_block < op.src_extent().start_block();
        });
    // upper bound: dst extent's start block > src extent's end block
    const auto upper_it = std::upper_bound(
        lower_it,
        operations_.end(),
        op,
        [](const CowMergeOperation& op, const CowMergeOperation& it) {
          auto src_end_block =
              op.src_extent().start_block() + op.src_extent().num_blocks() - 1;
          return src_end_block < it.dst_extent().start_block();
        });

    // TODO(xunchang) skip inserting the empty set to merge_after.
    if (lower_it == upper_it) {
      merge_after.insert({op, {}});
    } else {
      std::set<CowMergeOperation> operations(lower_it, upper_it);
      auto it = operations.find(op);
      if (it != operations.end()) {
        LOG(INFO) << "Self overlapping " << op;
        operations.erase(it);
      }
      auto ret = merge_after.emplace(op, std::move(operations));
      // Check the insertion indeed happens.
      CHECK(ret.second);
    }
  }

  *result = std::move(merge_after);
  return true;
}

bool MergeSequenceGenerator::Generate(
    std::vector<CowMergeOperation>* sequence) const {
  sequence->clear();
  std::map<CowMergeOperation, std::set<CowMergeOperation>> merge_after;
  if (!FindDependency(&merge_after)) {
    LOG(ERROR) << "Failed to find dependencies";
    return false;
  }

  LOG(INFO) << "Generating sequence";

  // Use the non-DFS version of the topology sort. So we can control the
  // operations to discard to break cycles; thus yielding a deterministic
  // sequence.
  std::map<CowMergeOperation, int> incoming_edges;
  for (const auto& it : merge_after) {
    for (const auto& blocked : it.second) {
      // Value is default initialized to 0.
      incoming_edges[blocked] += 1;
    }
  }

  std::set<CowMergeOperation> free_operations;
  for (const auto& op : operations_) {
    if (incoming_edges.find(op) == incoming_edges.end()) {
      free_operations.insert(op);
    }
  }

  std::vector<CowMergeOperation> merge_sequence;
  std::set<CowMergeOperation> convert_to_raw;
  while (!incoming_edges.empty()) {
    if (!free_operations.empty()) {
      merge_sequence.insert(
          merge_sequence.end(), free_operations.begin(), free_operations.end());
    } else {
      auto to_convert = incoming_edges.begin()->first;
      free_operations.insert(to_convert);
      convert_to_raw.insert(to_convert);
      LOG(INFO) << "Converting operation to raw " << to_convert;
    }

    std::set<CowMergeOperation> next_free_operations;
    for (const auto& op : free_operations) {
      incoming_edges.erase(op);

      // Now that this particular operation is merged, other operations blocked
      // by this one may be free. Decrement the count of blocking operations,
      // and set up the free operations for the next iteration.
      for (const auto& blocked : merge_after[op]) {
        auto it = incoming_edges.find(blocked);
        if (it == incoming_edges.end()) {
          continue;
        }

        auto blocking_transfer_count = &it->second;
        if (*blocking_transfer_count <= 0) {
          LOG(ERROR) << "Unexpected count in merge after map "
                     << blocking_transfer_count;
          return false;
        }
        // This operation is no longer blocked by anyone. Add it to the merge
        // sequence in the next iteration.
        *blocking_transfer_count -= 1;
        if (*blocking_transfer_count == 0) {
          next_free_operations.insert(blocked);
        }
      }
    }

    LOG(INFO) << "Remaining transfers " << incoming_edges.size()
              << ", free transfers " << free_operations.size()
              << ", merge_sequence size " << merge_sequence.size();
    free_operations = std::move(next_free_operations);
  }

  if (!free_operations.empty()) {
    merge_sequence.insert(
        merge_sequence.end(), free_operations.begin(), free_operations.end());
  }

  CHECK_EQ(operations_.size(), merge_sequence.size() + convert_to_raw.size());

  size_t blocks_in_sequence = 0;
  for (const CowMergeOperation& transfer : merge_sequence) {
    blocks_in_sequence += transfer.dst_extent().num_blocks();
  }

  size_t blocks_in_raw = 0;
  for (const CowMergeOperation& transfer : convert_to_raw) {
    blocks_in_raw += transfer.dst_extent().num_blocks();
  }

  LOG(INFO) << "Blocks in merge sequence " << blocks_in_sequence
            << ", blocks in raw " << blocks_in_raw;
  if (!ValidateSequence(merge_sequence)) {
    return false;
  }

  *sequence = std::move(merge_sequence);
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
