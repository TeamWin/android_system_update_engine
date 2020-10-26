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
  CHECK(buffer_.empty());
}

bool SnapshotExtentWriter::Init(
    FileDescriptorPtr /*fd*/,
    const google::protobuf::RepeatedPtrField<Extent>& extents,
    uint32_t /*block_size*/) {
  // TODO(zhangkelvin) Implement this
  return true;
}

// Returns true on success.
// This will construct a COW_REPLACE operation and forward it to CowWriter. It
// is important that caller does not perform SOURCE_COPY operation on this
// class, otherwise raw data will be stored. Caller should find ways to use
// COW_COPY whenever possible.
bool SnapshotExtentWriter::Write(const void* bytes, size_t count) {
  // TODO(zhangkelvin) Implement this
  return true;
}

}  // namespace chromeos_update_engine
