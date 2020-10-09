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

#include "update_engine/payload_consumer/vabc_partition_writer.h"

#include <memory>

#include <libsnapshot/cow_writer.h>

#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/extent_writer.h"
#include "update_engine/payload_consumer/install_plan.h"
#include "update_engine/payload_consumer/partition_writer.h"

namespace chromeos_update_engine {
bool VABCPartitionWriter::Init(const InstallPlan* install_plan,
                               bool source_may_exist) {
  TEST_AND_RETURN_FALSE(PartitionWriter::Init(install_plan, source_may_exist));

  // TODO(zhangkelvin) Add code specific to VABC. E.x. Convert InstallOps to
  // CowOps, perform all SOURCE_COPY upfront according to merge sequence.
  return true;
}

std::unique_ptr<ExtentWriter> VABCPartitionWriter::CreateBaseExtentWriter() {
  // TODO(zhangkelvin) Return a SnapshotExtentWriter
  return std::make_unique<DirectExtentWriter>();
}

[[nodiscard]] bool VABCPartitionWriter::PerformZeroOrDiscardOperation(
    const InstallOperation& operation) {
  // TODO(zhangkelvin) Create a COW_ZERO operation and send it to CowWriter
  return PartitionWriter::PerformZeroOrDiscardOperation(operation);
}

[[nodiscard]] bool VABCPartitionWriter::PerformSourceCopyOperation(
    const InstallOperation& operation, ErrorCode* error) {
  // TODO(zhangkelvin) Probably just ignore SOURCE_COPY? They should be taken
  // care of during Init();
  return true;
}

VABCPartitionWriter::~VABCPartitionWriter() = default;

}  // namespace chromeos_update_engine
