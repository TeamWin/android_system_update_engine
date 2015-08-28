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

#include "update_engine/install_plan.h"

#include <base/logging.h>

#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

const char* kLegacyPartitionNameKernel = "KERNEL";
const char* kLegacyPartitionNameRoot = "ROOT";

InstallPlan::InstallPlan(bool is_resume,
                         bool is_full_update,
                         const string& url,
                         uint64_t payload_size,
                         const string& payload_hash,
                         uint64_t metadata_size,
                         const string& metadata_signature,
                         const string& install_path,
                         const string& kernel_install_path,
                         const string& source_path,
                         const string& kernel_source_path,
                         const string& public_key_rsa)
    : is_resume(is_resume),
      is_full_update(is_full_update),
      download_url(url),
      payload_size(payload_size),
      payload_hash(payload_hash),
      metadata_size(metadata_size),
      metadata_signature(metadata_signature),
      install_path(install_path),
      kernel_install_path(kernel_install_path),
      source_path(source_path),
      kernel_source_path(kernel_source_path),
      kernel_size(0),
      rootfs_size(0),
      hash_checks_mandatory(false),
      powerwash_required(false),
      public_key_rsa(public_key_rsa) {}


bool InstallPlan::operator==(const InstallPlan& that) const {
  return ((is_resume == that.is_resume) &&
          (is_full_update == that.is_full_update) &&
          (download_url == that.download_url) &&
          (payload_size == that.payload_size) &&
          (payload_hash == that.payload_hash) &&
          (metadata_size == that.metadata_size) &&
          (metadata_signature == that.metadata_signature) &&
          (source_slot == that.source_slot) &&
          (target_slot == that.target_slot) &&
          (install_path == that.install_path) &&
          (kernel_install_path == that.kernel_install_path) &&
          (source_path == that.source_path) &&
          (kernel_source_path == that.kernel_source_path));
}

bool InstallPlan::operator!=(const InstallPlan& that) const {
  return !((*this) == that);
}

void InstallPlan::Dump() const {
  LOG(INFO) << "InstallPlan: "
            << (is_resume ? "resume" : "new_update")
            << ", payload type: " << (is_full_update ? "full" : "delta")
            << ", source_slot: " << BootControlInterface::SlotName(source_slot)
            << ", target_slot: " << BootControlInterface::SlotName(target_slot)
            << ", url: " << download_url
            << ", payload size: " << payload_size
            << ", payload hash: " << payload_hash
            << ", metadata size: " << metadata_size
            << ", metadata signature: " << metadata_signature
            << ", install_path: " << install_path
            << ", kernel_install_path: " << kernel_install_path
            << ", source_path: " << source_path
            << ", kernel_source_path: " << kernel_source_path
            << ", hash_checks_mandatory: " << utils::ToString(
                hash_checks_mandatory)
            << ", powerwash_required: " << utils::ToString(
                powerwash_required);
}

bool InstallPlan::LoadPartitionsFromSlots(SystemState* system_state) {
  bool result = true;
  if (source_slot != BootControlInterface::kInvalidSlot) {
    result = system_state->boot_control()->GetPartitionDevice(
        kLegacyPartitionNameRoot, source_slot, &source_path) && result;
    result = system_state->boot_control()->GetPartitionDevice(
        kLegacyPartitionNameKernel, source_slot, &kernel_source_path) && result;
  } else {
    source_path.clear();
    kernel_source_path.clear();
  }

  if (target_slot != BootControlInterface::kInvalidSlot) {
    result = system_state->boot_control()->GetPartitionDevice(
        kLegacyPartitionNameRoot, target_slot, &install_path) && result;
    result = system_state->boot_control()->GetPartitionDevice(
        kLegacyPartitionNameKernel, target_slot, &kernel_install_path) && result;
  } else {
    install_path.clear();
    kernel_install_path.clear();
  }
  return result;
}

}  // namespace chromeos_update_engine
