// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_EXTENT_UTILS_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_EXTENT_UTILS_H_

#include <vector>

#include "update_engine/update_metadata.pb.h"

// Utility functions for manipulating Extents and lists of blocks.

namespace chromeos_update_engine {

// |block| must either be the next block in the last extent or a block
// in the next extent. This function will not handle inserting block
// into an arbitrary place in the extents.
void AppendBlockToExtents(std::vector<Extent>* extents, uint64_t block);

// Get/SetElement are intentionally overloaded so that templated functions
// can accept either type of collection of Extents.
Extent GetElement(const std::vector<Extent>& collection, size_t index);
Extent GetElement(
    const google::protobuf::RepeatedPtrField<Extent>& collection,
    size_t index);

template<typename T>
uint64_t BlocksInExtents(const T& collection) {
  uint64_t ret = 0;
  for (size_t i = 0; i < static_cast<size_t>(collection.size()); ++i) {
    ret += GetElement(collection, i).num_blocks();
  }
  return ret;
}

bool operator==(const Extent& a, const Extent& b);

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_EXTENT_UTILS_H_
