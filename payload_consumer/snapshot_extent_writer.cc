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

#include "update_engine/payload_consumer/snapshot_extent_writer.h"

#include <algorithm>
#include <cstdint>

#include <libsnapshot/cow_writer.h>

#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

SnapshotExtentWriter::SnapshotExtentWriter(
    android::snapshot::ICowWriter* cow_writer)
    : cow_writer_(cow_writer) {
  CHECK_NE(cow_writer, nullptr);
}

SnapshotExtentWriter::~SnapshotExtentWriter() {
  CHECK(buffer_.empty()) << buffer_.size();
}

bool SnapshotExtentWriter::Init(
    const google::protobuf::RepeatedPtrField<Extent>& extents,
    uint32_t block_size) {
  extents_ = extents;
  cur_extent_idx_ = 0;
  buffer_.clear();
  buffer_.reserve(block_size);
  block_size_ = block_size;
  return true;
}

size_t SnapshotExtentWriter::ConsumeWithBuffer(const uint8_t* data,
                                               size_t count) {
  CHECK_LT(cur_extent_idx_, static_cast<size_t>(extents_.size()));
  const auto& cur_extent = extents_[cur_extent_idx_];
  const auto cur_extent_size = cur_extent.num_blocks() * block_size_;

  if (buffer_.empty() && count >= cur_extent_size) {
    if (!cow_writer_->AddRawBlocks(
            cur_extent.start_block(), data, cur_extent_size)) {
      LOG(ERROR) << "AddRawBlocks(" << cur_extent.start_block() << ", " << data
                 << ", " << cur_extent_size << ") failed.";
      // return value is expected to be greater than 0. Return 0 to signal error
      // condition
      return 0;
    }
    if (!next_extent()) {
      CHECK_EQ(count, cur_extent_size)
          << "Exhausted all blocks, but still have " << count - cur_extent_size
          << " bytes left";
    }
    return cur_extent_size;
  }
  CHECK_LT(buffer_.size(), cur_extent_size)
      << "Data left in buffer should never be >= cur_extent_size, otherwise "
         "we should have send that data to CowWriter. Buffer size: "
      << buffer_.size() << " current extent size: " << cur_extent_size;
  size_t bytes_to_copy =
      std::min<size_t>(count, cur_extent_size - buffer_.size());
  CHECK_GT(bytes_to_copy, 0U);

  buffer_.insert(buffer_.end(), data, data + bytes_to_copy);
  CHECK_LE(buffer_.size(), cur_extent_size);

  if (buffer_.size() == cur_extent_size) {
    if (!cow_writer_->AddRawBlocks(
            cur_extent.start_block(), buffer_.data(), buffer_.size())) {
      LOG(ERROR) << "AddRawBlocks(" << cur_extent.start_block() << ", "
                 << buffer_.data() << ", " << buffer_.size() << ") failed.";
      return 0;
    }
    buffer_.clear();
    if (!next_extent()) {
      CHECK_EQ(count, bytes_to_copy) << "Exhausted all blocks, but still have "
                                     << count - bytes_to_copy << " bytes left";
    }
  }
  return bytes_to_copy;
}

// Returns true on success.
// This will construct a COW_REPLACE operation and forward it to CowWriter. It
// is important that caller does not perform SOURCE_COPY operation on this
// class, otherwise raw data will be stored. Caller should find ways to use
// COW_COPY whenever possible.
bool SnapshotExtentWriter::Write(const void* bytes, size_t count) {
  if (count == 0) {
    return true;
  }
  CHECK_NE(extents_.size(), 0);

  auto data = static_cast<const uint8_t*>(bytes);
  while (count > 0) {
    auto bytes_written = ConsumeWithBuffer(data, count);
    TEST_AND_RETURN_FALSE(bytes_written > 0);
    data += bytes_written;
    count -= bytes_written;
  }
  return true;
}

bool SnapshotExtentWriter::next_extent() {
  cur_extent_idx_++;
  return cur_extent_idx_ < static_cast<size_t>(extents_.size());
}
}  // namespace chromeos_update_engine
