// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/extent_utils.h"

#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>

#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/annotated_operation.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

void AppendBlockToExtents(vector<Extent>* extents, uint64_t block) {
  // First try to extend the last extent in |extents|, if any.
  if (!extents->empty()) {
    Extent& extent = extents->back();
    uint64_t next_block = extent.start_block() == kSparseHole ?
        kSparseHole : extent.start_block() + extent.num_blocks();
    if (next_block == block) {
      extent.set_num_blocks(extent.num_blocks() + 1);
      return;
    }
  }
  // If unable to extend the last extent, append a new single-block extent.
  Extent new_extent;
  new_extent.set_start_block(block);
  new_extent.set_num_blocks(1);
  extents->push_back(new_extent);
}

Extent GetElement(const vector<Extent>& collection, size_t index) {
  return collection[index];
}
Extent GetElement(
    const google::protobuf::RepeatedPtrField<Extent>& collection,
    size_t index) {
  return collection.Get(index);
}

bool operator==(const Extent& a, const Extent& b) {
  return a.start_block() == b.start_block() && a.num_blocks() == b.num_blocks();
}

}  // namespace chromeos_update_engine
