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

#ifndef UPDATE_ENGINE_MOCK_PARTITION_WRITER_H_
#define UPDATE_ENGINE_MOCK_PARTITION_WRITER_H_

#include <gmock/gmock.h>

#include "common/error_code.h"
#include "payload_generator/delta_diff_generator.h"
#include "update_engine/payload_consumer/partition_writer.h"

namespace chromeos_update_engine {
class MockPartitionWriter : public PartitionWriter {
 public:
  MockPartitionWriter() : PartitionWriter({}, {}, nullptr, kBlockSize, false) {}
  virtual ~MockPartitionWriter() = default;

  // Perform necessary initialization work before InstallOperation can be
  // applied to this partition
  MOCK_METHOD(bool, Init, (const InstallPlan*, bool, size_t), (override));

  // |CheckpointUpdateProgress| will be called after SetNextOpIndex(), but it's
  // optional. DeltaPerformer may or may not call this everytime an operation is
  // applied.
  MOCK_METHOD(void, CheckpointUpdateProgress, (size_t), (override));

  // These perform a specific type of operation and return true on success.
  // |error| will be set if source hash mismatch, otherwise |error| might not be
  // set even if it fails.
  MOCK_METHOD(bool,
              PerformReplaceOperation,
              (const InstallOperation&, const void*, size_t),
              (override));
  MOCK_METHOD(bool,
              PerformZeroOrDiscardOperation,
              (const InstallOperation&),
              (override));
  MOCK_METHOD(bool,
              PerformSourceCopyOperation,
              (const InstallOperation&, ErrorCode*),
              (override));
  MOCK_METHOD(bool,
              PerformSourceBsdiffOperation,
              (const InstallOperation&, ErrorCode*, const void*, size_t),
              (override));
  MOCK_METHOD(bool,
              PerformPuffDiffOperation,
              (const InstallOperation&, ErrorCode*, const void*, size_t),
              (override));
};

}  // namespace chromeos_update_engine

#endif
