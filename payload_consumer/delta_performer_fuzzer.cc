//
// Copyright 2019 The Android Open Source Project
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

#include <string>

#include <base/logging.h>
#include <fuzzer/FuzzedDataProvider.h>

#include "update_engine/common/download_action.h"
#include "update_engine/common/fake_boot_control.h"
#include "update_engine/common/fake_hardware.h"
#include "update_engine/common/prefs.h"
#include "update_engine/payload_consumer/delta_performer.h"
#include "update_engine/payload_consumer/install_plan.h"

namespace chromeos_update_engine {

class FakeDownloadActionDelegate : public DownloadActionDelegate {
 public:
  FakeDownloadActionDelegate() = default;
  ~FakeDownloadActionDelegate() = default;

  // DownloadActionDelegate overrides;
  void BytesReceived(uint64_t bytes_progressed,
                     uint64_t bytes_received,
                     uint64_t total) override{};

  bool ShouldCancel(ErrorCode* cancel_reason) override { return false; };

  void DownloadComplete() override{};

  DISALLOW_COPY_AND_ASSIGN(FakeDownloadActionDelegate);
};

void FuzzDeltaPerformer(const uint8_t* data, size_t size) {
  MemoryPrefs prefs;
  FakeBootControl boot_control;
  FakeHardware hardware;
  FakeDownloadActionDelegate download_action_delegate;

  FuzzedDataProvider data_provider(data, size);

  InstallPlan install_plan{
      .target_slot = 1,
      .partitions = {InstallPlan::Partition{
          .source_path = "/dev/zero",
          .source_size = 4096,
          .target_path = "/dev/null",
          .target_size = 4096,
      }},
      .hash_checks_mandatory = true,
  };

  InstallPlan::Payload payload{
      .size = data_provider.ConsumeIntegralInRange<uint64_t>(0, 10000),
      .metadata_size = data_provider.ConsumeIntegralInRange<uint64_t>(0, 1000),
      .hash = data_provider.ConsumeBytes<uint8_t>(32),
      .type = static_cast<InstallPayloadType>(
          data_provider.ConsumeIntegralInRange(0, 3)),
      .already_applied = data_provider.ConsumeBool(),
  };

  DeltaPerformer performer(&prefs,
                           &boot_control,
                           &hardware,
                           &download_action_delegate,
                           &install_plan,
                           &payload,
                           data_provider.ConsumeBool());
  do {
    auto chunk_size = data_provider.ConsumeIntegralInRange<size_t>(0, 100);
    auto data = data_provider.ConsumeBytes<uint8_t>(chunk_size);
    if (!performer.Write(data.data(), data.size()))
      break;
  } while (data_provider.remaining_bytes() > 0);
}

}  // namespace chromeos_update_engine

class Environment {
 public:
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > 1000000) {
    return 0;
  }

  static Environment env;
  chromeos_update_engine::FuzzDeltaPerformer(data, size);
  return 0;
}
