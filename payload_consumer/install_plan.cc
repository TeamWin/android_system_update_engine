//
// Copyright (C) 2013 The Android Open Source Project
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

#include "update_engine/payload_consumer/install_plan.h"

#include <algorithm>
#include <utility>

#include <base/format_macros.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/payload_constants.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
string PayloadUrlsToString(
    const decltype(InstallPlan::Payload::payload_urls)& payload_urls) {
  return "(" + base::JoinString(payload_urls, ",") + ")";
}

string VectorToString(const vector<std::pair<string, string>>& input,
                      const string& separator) {
  vector<string> vec;
  std::transform(input.begin(),
                 input.end(),
                 std::back_inserter(vec),
                 [](const auto& pair) {
                   return base::JoinString({pair.first, pair.second}, ": ");
                 });
  return base::JoinString(vec, separator);
}
}  // namespace

string InstallPayloadTypeToString(InstallPayloadType type) {
  switch (type) {
    case InstallPayloadType::kUnknown:
      return "unknown";
    case InstallPayloadType::kFull:
      return "full";
    case InstallPayloadType::kDelta:
      return "delta";
  }
  return "invalid type";
}

bool InstallPlan::operator==(const InstallPlan& that) const {
  return ((is_resume == that.is_resume) &&
          (download_url == that.download_url) && (payloads == that.payloads) &&
          (source_slot == that.source_slot) &&
          (target_slot == that.target_slot) && (partitions == that.partitions));
}

bool InstallPlan::operator!=(const InstallPlan& that) const {
  return !((*this) == that);
}

void InstallPlan::Dump() const {
  LOG(INFO) << "InstallPlan: \n" << ToString();
}

string InstallPlan::ToString() const {
  string url_str = download_url;
  if (base::StartsWith(
          url_str, "fd://", base::CompareCase::INSENSITIVE_ASCII)) {
    int fd = std::stoi(url_str.substr(strlen("fd://")));
    url_str = utils::GetFilePath(fd);
  }

  vector<string> result_str;
  result_str.emplace_back(VectorToString(
      {
          {"type", (is_resume ? "resume" : "new_update")},
          {"version", version},
          {"source_slot", BootControlInterface::SlotName(source_slot)},
          {"target_slot", BootControlInterface::SlotName(target_slot)},
          {"initial url", url_str},
          {"hash_checks_mandatory", utils::ToString(hash_checks_mandatory)},
          {"powerwash_required", utils::ToString(powerwash_required)},
          {"switch_slot_on_reboot", utils::ToString(switch_slot_on_reboot)},
          {"run_post_install", utils::ToString(run_post_install)},
          {"is_rollback", utils::ToString(is_rollback)},
          {"rollback_data_save_requested",
           utils::ToString(rollback_data_save_requested)},
          {"write_verity", utils::ToString(write_verity)},
      },
      "\n"));

  for (const auto& partition : partitions) {
    result_str.emplace_back(VectorToString(
        {
            {"Partition", partition.name},
            {"source_size", base::NumberToString(partition.source_size)},
            {"source_path", partition.source_path},
            {"source_hash",
             base::HexEncode(partition.source_hash.data(),
                             partition.source_hash.size())},
            {"target_size", base::NumberToString(partition.target_size)},
            {"target_path", partition.target_path},
            {"target_hash",
             base::HexEncode(partition.target_hash.data(),
                             partition.target_hash.size())},
            {"run_postinstall", utils::ToString(partition.run_postinstall)},
            {"postinstall_path", partition.postinstall_path},
            {"readonly_target_path", partition.readonly_target_path},
            {"filesystem_type", partition.filesystem_type},
        },
        "\n  "));
  }

  for (unsigned int i = 0; i < payloads.size(); ++i) {
    const auto& payload = payloads[i];
    result_str.emplace_back(VectorToString(
        {
            {"Payload", base::NumberToString(i)},
            {"urls", PayloadUrlsToString(payload.payload_urls)},
            {"size", base::NumberToString(payload.size)},
            {"metadata_size", base::NumberToString(payload.metadata_size)},
            {"metadata_signature", payload.metadata_signature},
            {"hash", base::HexEncode(payload.hash.data(), payload.hash.size())},
            {"type", InstallPayloadTypeToString(payload.type)},
            {"fingerprint", payload.fp},
            {"app_id", payload.app_id},
            {"already_applied", utils::ToString(payload.already_applied)},
        },
        "\n  "));
  }

  return base::JoinString(result_str, "\n");
}

bool InstallPlan::LoadPartitionsFromSlots(BootControlInterface* boot_control) {
  bool result = true;
  for (Partition& partition : partitions) {
    if (source_slot != BootControlInterface::kInvalidSlot &&
        partition.source_size > 0) {
      TEST_AND_RETURN_FALSE(boot_control->GetPartitionDevice(
          partition.name, source_slot, &partition.source_path));
    } else {
      partition.source_path.clear();
    }

    if (target_slot != BootControlInterface::kInvalidSlot &&
        partition.target_size > 0) {
      auto device = boot_control->GetPartitionDevice(
          partition.name, target_slot, source_slot);
      TEST_AND_RETURN_FALSE(device.has_value());
      partition.target_path = device->rw_device_path;
      partition.readonly_target_path = device->readonly_device_path;
    } else {
      partition.target_path.clear();
    }
  }
  return result;
}

bool InstallPlan::Partition::operator==(
    const InstallPlan::Partition& that) const {
  return (name == that.name && source_path == that.source_path &&
          source_size == that.source_size && source_hash == that.source_hash &&
          target_path == that.target_path && target_size == that.target_size &&
          target_hash == that.target_hash &&
          run_postinstall == that.run_postinstall &&
          postinstall_path == that.postinstall_path &&
          filesystem_type == that.filesystem_type &&
          postinstall_optional == that.postinstall_optional);
}

}  // namespace chromeos_update_engine
