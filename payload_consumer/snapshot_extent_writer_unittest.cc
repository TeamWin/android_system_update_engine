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

#include <array>
#include <cstring>
#include <map>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>
#include <google/protobuf/message_lite.h>
#include <libsnapshot/cow_writer.h>

#include "update_engine/payload_consumer/snapshot_extent_writer.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

class FakeCowWriter : public android::snapshot::ICowWriter {
 public:
  struct CowOp {
    enum { COW_COPY, COW_REPLACE, COW_ZERO } type;
    std::vector<unsigned char> data;
    union {
      size_t source_block;
      size_t num_blocks;
    };
  };
  using ICowWriter::ICowWriter;
  ~FakeCowWriter() = default;

  bool EmitCopy(uint64_t new_block, uint64_t old_block) override {
    operations_[new_block] = {.type = CowOp::COW_COPY,
                              .source_block = static_cast<size_t>(old_block)};
    return true;
  }
  bool EmitRawBlocks(uint64_t new_block_start,
                     const void* data,
                     size_t size) override {
    auto&& op = operations_[new_block_start];
    const auto uint8_ptr = static_cast<const unsigned char*>(data);
    op.data.insert(op.data.end(), uint8_ptr, uint8_ptr + size);
    return true;
  }
  bool EmitZeroBlocks(uint64_t new_block_start, uint64_t num_blocks) override {
    operations_[new_block_start] = {.type = CowOp::COW_ZERO};
    return true;
  }
  bool Finalize() override {
    finalize_called_ = true;
    return true;
  }

  bool EmitLabel(uint64_t label) {
    label_count_++;
    return true;
  }

  // Return number of bytes the cow image occupies on disk.
  uint64_t GetCowSize() override {
    return std::accumulate(
        operations_.begin(), operations_.end(), 0, [](auto&& acc, auto&& op) {
          return acc + op.second.data.size();
        });
  }
  bool Contains(size_t block) {
    return operations_.find(block) != operations_.end();
  }
  bool finalize_called_ = true;
  size_t label_count_ = 0;
  std::map<size_t, CowOp> operations_;
};

class SnapshotExtentWriterTest : public ::testing::Test {
 public:
  void SetUp() override {}

 protected:
  android::snapshot::CowOptions options_ = {
      .block_size = static_cast<uint32_t>(kBlockSize)};
  FakeCowWriter cow_writer_{options_};
  SnapshotExtentWriter writer_{&cow_writer_};
};

void AddExtent(google::protobuf::RepeatedPtrField<Extent>* extents,
               size_t start_block,
               size_t num_blocks) {
  auto&& extent = extents->Add();
  extent->set_start_block(start_block);
  extent->set_num_blocks(num_blocks);
}

TEST_F(SnapshotExtentWriterTest, BufferWrites) {
  google::protobuf::RepeatedPtrField<Extent> extents;
  AddExtent(&extents, 123, 1);
  writer_.Init(extents, kBlockSize);

  std::vector<uint8_t> buf(kBlockSize, 0);
  buf[123] = 231;
  buf[231] = 123;
  buf[buf.size() - 1] = 255;

  writer_.Write(buf.data(), kBlockSize - 1);
  ASSERT_TRUE(cow_writer_.operations_.empty())
      << "Haven't send data of a complete block yet, CowWriter should not be "
         "invoked.";
  writer_.Write(buf.data() + kBlockSize - 1, 1);
  ASSERT_TRUE(cow_writer_.Contains(123))
      << "Once a block of data is sent to SnapshotExtentWriter, it should "
         "forward data to cow_writer.";
  ASSERT_EQ(cow_writer_.operations_.size(), 1U);
  ASSERT_EQ(buf, cow_writer_.operations_[123].data);
}

TEST_F(SnapshotExtentWriterTest, NonBufferedWrites) {
  google::protobuf::RepeatedPtrField<Extent> extents;
  AddExtent(&extents, 123, 1);
  AddExtent(&extents, 125, 1);
  writer_.Init(extents, kBlockSize);

  std::vector<uint8_t> buf(kBlockSize * 2, 0);
  buf[123] = 231;
  buf[231] = 123;
  buf[buf.size() - 1] = 255;

  writer_.Write(buf.data(), buf.size());
  ASSERT_TRUE(cow_writer_.Contains(123));
  ASSERT_TRUE(cow_writer_.Contains(125));

  ASSERT_EQ(cow_writer_.operations_.size(), 2U);
  auto actual_data = cow_writer_.operations_[123].data;
  actual_data.insert(actual_data.end(),
                     cow_writer_.operations_[125].data.begin(),
                     cow_writer_.operations_[125].data.end());
  ASSERT_EQ(buf, actual_data);
}

TEST_F(SnapshotExtentWriterTest, WriteAcrossBlockBoundary) {
  google::protobuf::RepeatedPtrField<Extent> extents;
  AddExtent(&extents, 123, 1);
  AddExtent(&extents, 125, 2);
  writer_.Init(extents, kBlockSize);

  std::vector<uint8_t> buf(kBlockSize * 3);
  std::memset(buf.data(), 0, buf.size());
  buf[123] = 231;
  buf[231] = 123;
  buf[buf.size() - 1] = 255;
  buf[kBlockSize - 1] = 254;

  writer_.Write(buf.data(), kBlockSize - 1);
  ASSERT_TRUE(cow_writer_.operations_.empty())
      << "Haven't send data of a complete block yet, CowWriter should not be "
         "invoked.";
  writer_.Write(buf.data() + kBlockSize - 1, 1 + kBlockSize * 2);
  ASSERT_TRUE(cow_writer_.Contains(123));
  ASSERT_TRUE(cow_writer_.Contains(125));

  ASSERT_EQ(cow_writer_.operations_.size(), 2U);
  auto actual_data = cow_writer_.operations_[123].data;
  actual_data.insert(actual_data.end(),
                     cow_writer_.operations_[125].data.begin(),
                     cow_writer_.operations_[125].data.end());
  ASSERT_EQ(buf, actual_data);
}
}  // namespace chromeos_update_engine
