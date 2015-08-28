//
// Copyright (C) 2011 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_INSTALL_PLAN_H_
#define UPDATE_ENGINE_INSTALL_PLAN_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/secure_blob.h>

#include "update_engine/action.h"
#include "update_engine/boot_control_interface.h"
#include "update_engine/system_state.h"

// InstallPlan is a simple struct that contains relevant info for many
// parts of the update system about the install that should happen.
namespace chromeos_update_engine {

// TODO(deymo): Remove these constants from this interface once the InstallPlan
// doesn't list the partitions explicitly.
extern const char* kLegacyPartitionNameKernel;
extern const char* kLegacyPartitionNameRoot;

struct InstallPlan {
  InstallPlan(bool is_resume,
              bool is_full_update,
              const std::string& url,
              uint64_t payload_size,
              const std::string& payload_hash,
              uint64_t metadata_size,
              const std::string& metadata_signature,
              const std::string& install_path,
              const std::string& kernel_install_path,
              const std::string& source_path,
              const std::string& kernel_source_path,
              const std::string& public_key_rsa);

  // Default constructor.
  InstallPlan() = default;

  bool operator==(const InstallPlan& that) const;
  bool operator!=(const InstallPlan& that) const;

  void Dump() const;

  bool LoadPartitionsFromSlots(SystemState* system_state);

  bool is_resume{false};
  bool is_full_update{false};
  std::string download_url;  // url to download from
  std::string version;       // version we are installing.

  uint64_t payload_size{0};              // size of the payload
  std::string payload_hash;              // SHA256 hash of the payload
  uint64_t metadata_size{0};             // size of the metadata
  std::string metadata_signature;        // signature of the  metadata

  // The partition slots used for the update.
  BootControlInterface::Slot source_slot{BootControlInterface::kInvalidSlot};
  BootControlInterface::Slot target_slot{BootControlInterface::kInvalidSlot};

  // TODO(deymo): Deprecate these fields and use the slots instead.
  std::string install_path;              // path to install device
  std::string kernel_install_path;       // path to kernel install device
  std::string source_path;               // path to source device
  std::string kernel_source_path;        // path to source kernel device

  // The fields below are used for kernel and rootfs verification. The flow is:
  //
  // 1. FilesystemVerifierAction computes and fills in the source partition
  // sizes and hashes.
  //
  // 2. DownloadAction verifies the source partition sizes and hashes against
  // the expected values transmitted in the update manifest. It fills in the
  // expected applied partition sizes and hashes based on the manifest.
  //
  // 3. FilesystemVerifierAction computes and verifies the applied and source
  // partition sizes and hashes against the expected values.
  uint64_t kernel_size{0};
  uint64_t rootfs_size{0};
  chromeos::Blob kernel_hash;
  chromeos::Blob rootfs_hash;
  chromeos::Blob source_kernel_hash;
  chromeos::Blob source_rootfs_hash;

  // True if payload hash checks are mandatory based on the system state and
  // the Omaha response.
  bool hash_checks_mandatory{false};

  // True if Powerwash is required on reboot after applying the payload.
  // False otherwise.
  bool powerwash_required{false};

  // If not blank, a base-64 encoded representation of the PEM-encoded
  // public key in the response.
  std::string public_key_rsa;
};

class InstallPlanAction;

template<>
class ActionTraits<InstallPlanAction> {
 public:
  // Takes the install plan as input
  typedef InstallPlan InputObjectType;
  // Passes the install plan as output
  typedef InstallPlan OutputObjectType;
};

// Basic action that only receives and sends Install Plans.
// Can be used to construct an Install Plan to send to any other Action that
// accept an InstallPlan.
class InstallPlanAction : public Action<InstallPlanAction> {
 public:
  InstallPlanAction() {}
  explicit InstallPlanAction(const InstallPlan& install_plan):
    install_plan_(install_plan) {}

  void PerformAction() override {
    if (HasOutputPipe()) {
      SetOutputObject(install_plan_);
    }
    processor_->ActionComplete(this, ErrorCode::kSuccess);
  }

  InstallPlan* install_plan() { return &install_plan_; }

  static std::string StaticType() { return "InstallPlanAction"; }
  std::string Type() const override { return StaticType(); }

  typedef ActionTraits<InstallPlanAction>::InputObjectType InputObjectType;
  typedef ActionTraits<InstallPlanAction>::OutputObjectType OutputObjectType;

 private:
  InstallPlan install_plan_;

  DISALLOW_COPY_AND_ASSIGN(InstallPlanAction);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_INSTALL_PLAN_H_
