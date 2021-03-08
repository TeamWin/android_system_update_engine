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

#include "update_engine/common/cow_operation_convert.h"

#include <base/logging.h>

#include "update_engine/payload_generator/extent_ranges.h"
#include "update_engine/payload_generator/extent_utils.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

std::vector<CowOperation> ConvertToCowOperations(
    const ::google::protobuf::RepeatedPtrField<
        ::chromeos_update_engine::InstallOperation>& operations,
    const ::google::protobuf::RepeatedPtrField<CowMergeOperation>&
        merge_operations) {
  ExtentRanges merge_extents;
  std::vector<CowOperation> converted;
  ExtentRanges modified_extents;

  // We want all CowCopy ops to be done first, before any COW_REPLACE happen.
  // Therefore we add these ops in 2 separate loops. This is because during
  // merge, a CowReplace might modify a block needed by CowCopy, so we always
  // perform CowCopy first.

  // This loop handles CowCopy blocks within SOURCE_COPY, and the next loop
  // converts the leftover blocks to CowReplace?
  for (const auto& merge_op : merge_operations) {
    if (merge_op.type() != CowMergeOperation::COW_COPY) {
      continue;
    }
    merge_extents.AddExtent(merge_op.dst_extent());
    const auto& src_extent = merge_op.src_extent();
    const auto& dst_extent = merge_op.dst_extent();
    // Add blocks in reverse order, because snapused specifically prefers this
    // ordering. Since we already eliminated all self-overlapping SOURCE_COPY
    // during delta generation, this should be safe to do.
    for (uint64_t i = src_extent.num_blocks(); i > 0; i--) {
      auto src_block = src_extent.start_block() + i - 1;
      auto dst_block = dst_extent.start_block() + i - 1;
      converted.push_back({CowOperation::CowCopy, src_block, dst_block});
      modified_extents.AddBlock(dst_block);
    }
  }
  // COW_REPLACE are added after COW_COPY, because replace might modify blocks
  // needed by COW_COPY. Please don't merge this loop with the previous one.
  for (const auto& operation : operations) {
    if (operation.type() != InstallOperation::SOURCE_COPY) {
      continue;
    }
    const auto& src_extents = operation.src_extents();
    const auto& dst_extents = operation.dst_extents();
    BlockIterator it1{src_extents};
    BlockIterator it2{dst_extents};
    while (!it1.is_end() && !it2.is_end()) {
      auto src_block = *it1;
      auto dst_block = *it2;
      if (!merge_extents.ContainsBlock(dst_block)) {
        converted.push_back({CowOperation::CowReplace, src_block, dst_block});
      }
      ++it1;
      ++it2;
    }
  }
  return converted;
}
}  // namespace chromeos_update_engine
