//
// Copyright (C) 2017 The Android Open Source Project
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

#include "update_engine/payload_consumer/file_descriptor_utils.h"

#include <algorithm>

#include <base/logging.h>

#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/extent_writer.h"

using google::protobuf::RepeatedPtrField;
using std::min;

namespace chromeos_update_engine {

namespace {

// Size of the buffer used to copy blocks.
const int kMaxCopyBufferSize = 1024 * 1024;

// Return the total number of blocks in the passed |extents| list.
uint64_t GetBlockCount(const RepeatedPtrField<Extent>& extents) {
  uint64_t sum = 0;
  for (const Extent& ext : extents) {
    sum += ext.num_blocks();
  }
  return sum;
}

}  // namespace

namespace fd_utils {

bool CopyAndHashExtents(FileDescriptorPtr source,
                        const RepeatedPtrField<Extent>& src_extents,
                        FileDescriptorPtr target,
                        const RepeatedPtrField<Extent>& tgt_extents,
                        uint32_t block_size,
                        brillo::Blob* hash_out) {
  HashCalculator source_hasher;

  uint64_t buffer_blocks = kMaxCopyBufferSize / block_size;
  // Ensure we copy at least one block at a time.
  if (buffer_blocks < 1)
    buffer_blocks = 1;

  uint64_t total_blocks = GetBlockCount(src_extents);
  TEST_AND_RETURN_FALSE(total_blocks == GetBlockCount(tgt_extents));

  brillo::Blob buf(buffer_blocks * block_size);

  DirectExtentWriter writer;
  std::vector<Extent> vec_tgt_extents;
  vec_tgt_extents.reserve(tgt_extents.size());
  for (const auto& ext : tgt_extents) {
    vec_tgt_extents.push_back(ext);
  }
  TEST_AND_RETURN_FALSE(writer.Init(target, vec_tgt_extents, block_size));

  for (const Extent& src_ext : src_extents) {
    for (uint64_t src_ext_block = 0; src_ext_block < src_ext.num_blocks();
         src_ext_block += buffer_blocks) {
      uint64_t iteration_blocks =
          min(buffer_blocks,
              static_cast<uint64_t>(src_ext.num_blocks() - src_ext_block));
      uint64_t src_start_block = src_ext.start_block() + src_ext_block;

      ssize_t bytes_read_this_iteration;
      TEST_AND_RETURN_FALSE(utils::PReadAll(source,
                                            buf.data(),
                                            iteration_blocks * block_size,
                                            src_start_block * block_size,
                                            &bytes_read_this_iteration));

      TEST_AND_RETURN_FALSE(
          bytes_read_this_iteration ==
          static_cast<ssize_t>(iteration_blocks * block_size));

      TEST_AND_RETURN_FALSE(
          writer.Write(buf.data(), iteration_blocks * block_size));

      if (hash_out) {
        TEST_AND_RETURN_FALSE(
            source_hasher.Update(buf.data(), iteration_blocks * block_size));
      }
    }
  }
  TEST_AND_RETURN_FALSE(writer.End());

  if (hash_out) {
    TEST_AND_RETURN_FALSE(source_hasher.Finalize());
    *hash_out = source_hasher.raw_hash();
  }
  return true;
}

}  // namespace fd_utils

}  // namespace chromeos_update_engine
