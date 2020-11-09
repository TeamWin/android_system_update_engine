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

#ifndef UPDATE_ENGINE_PARTITION_WRITER_H_
#define UPDATE_ENGINE_PARTITION_WRITER_H_

#include <cstdint>
#include <memory>
#include <string>

#include <brillo/secure_blob.h>
#include <gtest/gtest_prod.h>

#include "update_engine/common/dynamic_partition_control_interface.h"
#include "update_engine/common/prefs_interface.h"
#include "update_engine/payload_consumer/extent_writer.h"
#include "update_engine/payload_consumer/file_descriptor.h"
#include "update_engine/payload_consumer/install_plan.h"
#include "update_engine/update_metadata.pb.h"
namespace chromeos_update_engine {
class PartitionWriter {
 public:
  PartitionWriter(const PartitionUpdate& partition_update,
                  const InstallPlan::Partition& install_part,
                  DynamicPartitionControlInterface* dynamic_control,
                  size_t block_size,
                  PrefsInterface* prefs,
                  bool is_interactive);
  virtual ~PartitionWriter();
  static bool ValidateSourceHash(const brillo::Blob& calculated_hash,
                                 const InstallOperation& operation,
                                 const FileDescriptorPtr source_fd,
                                 ErrorCode* error);

  // Perform necessary initialization work before InstallOperation can be
  // applied to this partition
  [[nodiscard]] virtual bool Init(const InstallPlan* install_plan,
                                  bool source_may_exist);

  // This will be called by DeltaPerformer after applying an InstallOp.
  // |next_op_index| is index of next operation that should be applied.
  // |next_op_index-1| is the last operation that is already applied.
  virtual void CheckpointUpdateProgress(size_t next_op_index) {}

  int Close();

  // These perform a specific type of operation and return true on success.
  // |error| will be set if source hash mismatch, otherwise |error| might not be
  // set even if it fails.
  [[nodiscard]] virtual bool PerformReplaceOperation(
      const InstallOperation& operation, const void* data, size_t count);
  [[nodiscard]] virtual bool PerformZeroOrDiscardOperation(
      const InstallOperation& operation);

  [[nodiscard]] virtual bool PerformSourceCopyOperation(
      const InstallOperation& operation, ErrorCode* error);
  [[nodiscard]] virtual bool PerformSourceBsdiffOperation(
      const InstallOperation& operation,
      ErrorCode* error,
      const void* data,
      size_t count);
  [[nodiscard]] virtual bool PerformPuffDiffOperation(
      const InstallOperation& operation,
      ErrorCode* error,
      const void* data,
      size_t count);
  [[nodiscard]] virtual bool Flush();

 protected:
  friend class PartitionWriterTest;
  FRIEND_TEST(PartitionWriterTest, ChooseSourceFDTest);

  bool OpenCurrentECCPartition();
  // For a given operation, choose the source fd to be used (raw device or error
  // correction device) based on the source operation hash.
  // Returns nullptr if the source hash mismatch cannot be corrected, and set
  // the |error| accordingly.
  FileDescriptorPtr ChooseSourceFD(const InstallOperation& operation,
                                   ErrorCode* error);
  [[nodiscard]] virtual std::unique_ptr<ExtentWriter> CreateBaseExtentWriter();

  const PartitionUpdate& partition_update_;
  const InstallPlan::Partition& install_part_;
  DynamicPartitionControlInterface* dynamic_control_;
  // Path to source partition
  std::string source_path_;
  // Path to target partition
  std::string target_path_;
  FileDescriptorPtr source_fd_;
  FileDescriptorPtr target_fd_;
  const bool interactive_;
  const size_t block_size_;
  // File descriptor of the error corrected source partition. Only set while
  // updating partition using a delta payload for a partition where error
  // correction is available. The size of the error corrected device is smaller
  // than the underlying raw device, since it doesn't include the error
  // correction blocks.
  FileDescriptorPtr source_ecc_fd_{nullptr};

  // The total number of operations that failed source hash verification but
  // passed after falling back to the error-corrected |source_ecc_fd_| device.
  uint64_t source_ecc_recovered_failures_{0};

  // Whether opening the current partition as an error-corrected device failed.
  // Used to avoid re-opening the same source partition if it is not actually
  // error corrected.
  bool source_ecc_open_failure_{false};

  PrefsInterface* prefs_;
};

namespace partition_writer {
// Return a PartitionWriter instance for perform InstallOps on this partition.
// Uses VABCPartitionWriter for Virtual AB Compression
std::unique_ptr<PartitionWriter> CreatePartitionWriter(
    const PartitionUpdate& partition_update,
    const InstallPlan::Partition& install_part,
    DynamicPartitionControlInterface* dynamic_control,
    size_t block_size,
    PrefsInterface* prefs,
    bool is_interactive,
    bool is_dynamic_partition);
}  // namespace partition_writer
}  // namespace chromeos_update_engine

#endif
