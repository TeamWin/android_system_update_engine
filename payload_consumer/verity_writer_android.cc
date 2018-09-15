//
// Copyright (C) 2018 The Android Open Source Project
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

#include "update_engine/payload_consumer/verity_writer_android.h"

#include <fcntl.h>

#include <algorithm>
#include <memory>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

#include "update_engine/common/utils.h"

namespace chromeos_update_engine {

namespace verity_writer {
std::unique_ptr<VerityWriterInterface> CreateVerityWriter() {
  return std::make_unique<VerityWriterAndroid>();
}
}  // namespace verity_writer

bool VerityWriterAndroid::Init(const InstallPlan::Partition& partition) {
  partition_ = &partition;

  if (partition_->hash_tree_size != 0) {
    auto hash_function =
        HashTreeBuilder::HashFunction(partition_->hash_tree_algorithm);
    if (hash_function == nullptr) {
      LOG(ERROR) << "Verity hash algorithm not supported: "
                 << partition_->hash_tree_algorithm;
      return false;
    }
    hash_tree_builder_ = std::make_unique<HashTreeBuilder>(
        partition_->block_size, hash_function);
    TEST_AND_RETURN_FALSE(hash_tree_builder_->Initialize(
        partition_->hash_tree_data_size, partition_->hash_tree_salt));
    if (hash_tree_builder_->CalculateSize(partition_->hash_tree_data_size) !=
        partition_->hash_tree_size) {
      LOG(ERROR) << "Verity hash tree size does not match, stored: "
                 << partition_->hash_tree_size << ", calculated: "
                 << hash_tree_builder_->CalculateSize(
                        partition_->hash_tree_data_size);
      return false;
    }
  }
  return true;
}

bool VerityWriterAndroid::Update(uint64_t offset,
                                 const uint8_t* buffer,
                                 size_t size) {
  if (partition_->hash_tree_size != 0) {
    uint64_t hash_tree_data_end =
        partition_->hash_tree_data_offset + partition_->hash_tree_data_size;
    uint64_t start_offset = std::max(offset, partition_->hash_tree_data_offset);
    uint64_t end_offset = std::min(offset + size, hash_tree_data_end);
    if (start_offset < end_offset) {
      TEST_AND_RETURN_FALSE(hash_tree_builder_->Update(
          buffer + start_offset - offset, end_offset - start_offset));

      if (end_offset == hash_tree_data_end) {
        // All hash tree data blocks has been hashed, write hash tree to disk.
        int fd = HANDLE_EINTR(open(partition_->target_path.c_str(), O_WRONLY));
        if (fd < 0) {
          PLOG(ERROR) << "Failed to open " << partition_->target_path
                      << " to write verity data.";
          return false;
        }
        ScopedFdCloser fd_closer(&fd);

        LOG(INFO) << "Writing verity hash tree to " << partition_->target_path;
        TEST_AND_RETURN_FALSE(hash_tree_builder_->BuildHashTree());
        TEST_AND_RETURN_FALSE(hash_tree_builder_->WriteHashTreeToFd(
            fd, partition_->hash_tree_offset));
        hash_tree_builder_.reset();
      }
    }
  }
  if (partition_->fec_size != 0) {
    // TODO(senj): Update FEC data.
  }
  return true;
}

}  // namespace chromeos_update_engine
