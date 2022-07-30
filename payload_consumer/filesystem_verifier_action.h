//
// Copyright (C) 2012 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_PAYLOAD_CONSUMER_FILESYSTEM_VERIFIER_ACTION_H_
#define UPDATE_ENGINE_PAYLOAD_CONSUMER_FILESYSTEM_VERIFIER_ACTION_H_

#include <sys/stat.h>
#include <sys/types.h>

#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <brillo/message_loops/message_loop.h>

#include "update_engine/common/action.h"
#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/scoped_task_id.h"
#include "update_engine/payload_consumer/file_descriptor.h"
#include "update_engine/payload_consumer/install_plan.h"
#include "update_engine/payload_consumer/verity_writer_interface.h"

// This action will hash all the partitions of the target slot involved in the
// update. The hashes are then verified against the ones in the InstallPlan.
// If the target hash does not match, the action will fail. In case of failure,
// the error code will depend on whether the source slot hashes are provided and
// match.

namespace chromeos_update_engine {

// The step FilesystemVerifier is on. On kVerifyTargetHash it computes the hash
// on the target partitions based on the already populated size and verifies it
// matches the one in the target_hash in the InstallPlan.
// If the hash matches, then we skip the kVerifySourceHash step, otherwise we
// need to check if the source is the root cause of the mismatch.
enum class VerifierStep {
  kVerifyTargetHash,
  kVerifySourceHash,
};

class FilesystemVerifyDelegate {
 public:
  virtual ~FilesystemVerifyDelegate() = default;
  virtual void OnVerifyProgressUpdate(double progress) = 0;
};

class FilesystemVerifierAction : public InstallPlanAction {
 public:
  explicit FilesystemVerifierAction(
      DynamicPartitionControlInterface* dynamic_control)
      : verity_writer_(verity_writer::CreateVerityWriter()),
        dynamic_control_(dynamic_control) {
    CHECK(dynamic_control_);
  }

  ~FilesystemVerifierAction() override = default;

  void PerformAction() override;
  void TerminateProcessing() override;

  // Used for listening to progress updates
  void set_delegate(FilesystemVerifyDelegate* delegate) {
    this->delegate_ = delegate;
  }
  [[nodiscard]] FilesystemVerifyDelegate* get_delegate() const {
    return this->delegate_;
  }

  // Debugging/logging
  static std::string StaticType() { return "FilesystemVerifierAction"; }
  std::string Type() const override { return StaticType(); }

 private:
  friend class FilesystemVerifierActionTestDelegate;
  void WriteVerityAndHashPartition(FileDescriptorPtr fd,
                                   const off64_t start_offset,
                                   const off64_t end_offset,
                                   void* buffer,
                                   const size_t buffer_size);
  void HashPartition(FileDescriptorPtr fd,
                     const off64_t start_offset,
                     const off64_t end_offset,
                     void* buffer,
                     const size_t buffer_size);

  // Return true if we need to write verity bytes.
  bool ShouldWriteVerity();
  // Starts the hashing of the current partition. If there aren't any partitions
  // remaining to be hashed, it finishes the action.
  void StartPartitionHashing();
  
  // Used to check the device ram type is LPDDRX4 or LPDDR5
  // by reading the value of ro.boot.ddr_type
  // will default to false if prop is missing
  bool IsDDR5();
  int read_file(std::string fn, std::string& results);
  bool Path_Exists(std::string Path);

  // Used to indicated if specific LPDRX images exist in the payload
  // Used in some Oneplus models (Oneplus 8T and Oneplus 9R) that can have either LPDDRX4 or LPDDR5 ram
  bool xbllp5PartitionsExist = false;
  
  const std::string& GetPartitionPath() const;

  bool IsVABC(const InstallPlan::Partition& partition) const;

  size_t GetPartitionSize() const;

  // When the read is done, finalize the hash checking of the current partition
  // and continue checking the next one.
  void FinishPartitionHashing();

  // Cleans up all the variables we use for async operations and tells the
  // ActionProcessor we're done w/ |code| as passed in. |cancelled_| should be
  // true if TerminateProcessing() was called.
  void Cleanup(ErrorCode code);

  // Invoke delegate callback to report progress, if delegate is not null
  void UpdateProgress(double progress);

  // Updates progress of current partition. |progress| should be in range [0,
  // 1], and it will be scaled appropriately with # of partitions.
  void UpdatePartitionProgress(double progress);

  // Initialize read_fd_ and write_fd_
  bool InitializeFd(const std::string& part_path);
  bool InitializeFdVABC(bool should_write_verity);

  // The type of the partition that we are verifying.
  VerifierStep verifier_step_ = VerifierStep::kVerifyTargetHash;

  // The index in the install_plan_.partitions vector of the partition currently
  // being hashed.
  size_t partition_index_{0};

  // If not null, the FileDescriptor used to read from the device.
  // verity writer might attempt to write to this fd, if verity is enabled.
  FileDescriptorPtr partition_fd_;

  // Buffer for storing data we read.
  brillo::Blob buffer_;

  bool cancelled_{false};  // true if the action has been cancelled.

  // Calculates the hash of the data.
  std::unique_ptr<HashCalculator> hasher_;

  // Write verity data of the current partition.
  std::unique_ptr<VerityWriterInterface> verity_writer_;

  // Verifies the untouched dynamic partitions for partial updates.
  DynamicPartitionControlInterface* dynamic_control_{nullptr};

  // Reads and hashes this many bytes from the head of the input stream. When
  // the partition starts to be hashed, this field is initialized from the
  // corresponding InstallPlan::Partition size which is the total size
  // update_engine is expected to write, and may be smaller than the size of the
  // partition in gpt.
  uint64_t partition_size_{0};

  // The byte offset that we are reading in the current partition.
  uint64_t offset_{0};

  // The end offset of filesystem data, first byte position of hashtree.
  uint64_t filesystem_data_end_{0};

  // An observer that observes progress updates of this action.
  FilesystemVerifyDelegate* delegate_{};

  // Callback that should be cancelled on |TerminateProcessing|. Usually this
  // points to pending read callbacks from async stream.
  ScopedTaskId pending_task_id_;

  DISALLOW_COPY_AND_ASSIGN(FilesystemVerifierAction);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_CONSUMER_FILESYSTEM_VERIFIER_ACTION_H_
