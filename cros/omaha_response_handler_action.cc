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

#include "update_engine/cros/omaha_response_handler_action.h"

#include <limits>
#include <string>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/version.h>
#include <policy/device_policy.h>

#include "update_engine/common/constants.h"
#include "update_engine/common/hardware_interface.h"
#include "update_engine/common/prefs_interface.h"
#include "update_engine/common/system_state.h"
#include "update_engine/common/utils.h"
#include "update_engine/cros/connection_manager_interface.h"
#include "update_engine/cros/omaha_request_params.h"
#include "update_engine/cros/payload_state_interface.h"
#include "update_engine/payload_consumer/delta_performer.h"
#include "update_engine/update_manager/policy.h"
#include "update_engine/update_manager/update_manager.h"

using chromeos_update_manager::kRollforwardInfinity;
using chromeos_update_manager::Policy;
using chromeos_update_manager::UpdateManager;
using std::numeric_limits;
using std::string;

namespace chromeos_update_engine {

OmahaResponseHandlerAction::OmahaResponseHandlerAction()
    : deadline_file_(constants::kOmahaResponseDeadlineFile) {}

void OmahaResponseHandlerAction::PerformAction() {
  CHECK(HasInputObject());
  ScopedActionCompleter completer(processor_, this);
  const OmahaResponse& response = GetInputObject();
  if (!response.update_exists) {
    LOG(INFO) << "There are no updates. Aborting.";
    completer.set_code(ErrorCode::kNoUpdate);
    return;
  }

  // All decisions as to which URL should be used have already been done. So,
  // make the current URL as the download URL.
  string current_url = SystemState::Get()->payload_state()->GetCurrentUrl();
  if (current_url.empty()) {
    // This shouldn't happen as we should always supply the HTTPS backup URL.
    // Handling this anyway, just in case.
    LOG(ERROR) << "There are no suitable URLs in the response to use.";
    completer.set_code(ErrorCode::kOmahaResponseInvalid);
    return;
  }

  // This is the url to the first package, not all packages.
  // (For updates): All |Action|s prior to this must pass in non-excluded URLs
  // within the |OmahaResponse|, reference exlusion logic in
  // |OmahaRequestAction| and keep the enforcement of exclusions for updates.
  install_plan_.download_url = current_url;
  install_plan_.version = response.version;

  OmahaRequestParams* const params = SystemState::Get()->request_params();
  PayloadStateInterface* const payload_state =
      SystemState::Get()->payload_state();

  // If we're using p2p to download and there is a local peer, use it.
  if (payload_state->GetUsingP2PForDownloading() &&
      !payload_state->GetP2PUrl().empty()) {
    LOG(INFO) << "Replacing URL " << install_plan_.download_url
              << " with local URL " << payload_state->GetP2PUrl()
              << " since p2p is enabled.";
    install_plan_.download_url = payload_state->GetP2PUrl();
    payload_state->SetUsingP2PForDownloading(true);
  }

  // Fill up the other properties based on the response.
  string update_check_response_hash;
  for (const auto& package : response.packages) {
    brillo::Blob raw_hash;
    if (!base::HexStringToBytes(package.hash, &raw_hash)) {
      LOG(ERROR) << "Failed to convert payload hash from hex string to bytes: "
                 << package.hash;
      completer.set_code(ErrorCode::kOmahaResponseInvalid);
      return;
    }
    install_plan_.payloads.push_back(
        {.payload_urls = package.payload_urls,
         .size = package.size,
         .metadata_size = package.metadata_size,
         .metadata_signature = package.metadata_signature,
         .hash = raw_hash,
         .type = package.is_delta ? InstallPayloadType::kDelta
                                  : InstallPayloadType::kFull,
         .fp = package.fp,
         .app_id = package.app_id});
    update_check_response_hash += package.hash + ":";
  }
  install_plan_.public_key_rsa = response.public_key_rsa;
  install_plan_.hash_checks_mandatory = AreHashChecksMandatory(response);
  install_plan_.is_resume = DeltaPerformer::CanResumeUpdate(
      SystemState::Get()->prefs(), update_check_response_hash);
  if (install_plan_.is_resume) {
    payload_state->UpdateResumed();
  } else {
    payload_state->UpdateRestarted();
    LOG_IF(WARNING,
           !DeltaPerformer::ResetUpdateProgress(SystemState::Get()->prefs(),
                                                false))
        << "Unable to reset the update progress.";
    LOG_IF(WARNING,
           !SystemState::Get()->prefs()->SetString(
               kPrefsUpdateCheckResponseHash, update_check_response_hash))
        << "Unable to save the update check response hash.";
  }

  if (params->is_install()) {
    install_plan_.target_slot =
        SystemState::Get()->boot_control()->GetCurrentSlot();
    install_plan_.source_slot = BootControlInterface::kInvalidSlot;
  } else {
    install_plan_.source_slot =
        SystemState::Get()->boot_control()->GetCurrentSlot();
    install_plan_.target_slot = install_plan_.source_slot == 0 ? 1 : 0;
  }

  // The Omaha response doesn't include the channel name for this image, so we
  // use the download_channel we used during the request to tag the target slot.
  // This will be used in the next boot to know the channel the image was
  // downloaded from.
  string current_channel_key =
      kPrefsChannelOnSlotPrefix + std::to_string(install_plan_.target_slot);
  SystemState::Get()->prefs()->SetString(current_channel_key,
                                         params->download_channel());

  // Checking whether device is able to boot up the returned rollback image.
  if (response.is_rollback) {
    if (!params->rollback_allowed()) {
      LOG(ERROR) << "Received rollback image but rollback is not allowed.";
      completer.set_code(ErrorCode::kOmahaResponseInvalid);
      return;
    }

    // Calculate the values on the version values on current device.
    auto min_kernel_key_version = static_cast<uint32_t>(
        SystemState::Get()->hardware()->GetMinKernelKeyVersion());
    auto min_firmware_key_version = static_cast<uint32_t>(
        SystemState::Get()->hardware()->GetMinFirmwareKeyVersion());

    uint32_t kernel_key_version =
        static_cast<uint32_t>(response.rollback_key_version.kernel_key) << 16 |
        static_cast<uint32_t>(response.rollback_key_version.kernel);
    uint32_t firmware_key_version =
        static_cast<uint32_t>(response.rollback_key_version.firmware_key)
            << 16 |
        static_cast<uint32_t>(response.rollback_key_version.firmware);

    LOG(INFO) << "Rollback image versions:"
              << " device_kernel_key_version=" << min_kernel_key_version
              << " image_kernel_key_version=" << kernel_key_version
              << " device_firmware_key_version=" << min_firmware_key_version
              << " image_firmware_key_version=" << firmware_key_version;

    // Don't attempt a rollback if the versions are incompatible or the
    // target image does not specify the version information.
    if (kernel_key_version == numeric_limits<uint32_t>::max() ||
        firmware_key_version == numeric_limits<uint32_t>::max() ||
        kernel_key_version < min_kernel_key_version ||
        firmware_key_version < min_firmware_key_version) {
      LOG(ERROR) << "Device won't be able to boot up the rollback image.";
      completer.set_code(ErrorCode::kRollbackNotPossible);
      return;
    }
    install_plan_.is_rollback = true;
    install_plan_.rollback_data_save_requested =
        params->rollback_data_save_requested();
  }

  // Powerwash if either the response requires it or the parameters indicated
  // powerwash (usually because there was a channel downgrade) and we are
  // downgrading the version. Enterprise rollback, indicated by
  // |response.is_rollback| is dealt with separately above.
  if (response.powerwash_required) {
    install_plan_.powerwash_required = true;
  } else if (params->ShouldPowerwash() && !response.is_rollback) {
    base::Version new_version(response.version);
    base::Version current_version(params->app_version());

    if (!new_version.IsValid()) {
      LOG(WARNING) << "Not powerwashing,"
                   << " the update's version number is unreadable."
                   << " Update's version number: " << response.version;
    } else if (!current_version.IsValid()) {
      LOG(WARNING) << "Not powerwashing,"
                   << " the current version number is unreadable."
                   << " Current version number: " << params->app_version();
    } else if (new_version < current_version) {
      install_plan_.powerwash_required = true;
      // Always try to preserve enrollment and wifi data for enrolled devices.
      install_plan_.rollback_data_save_requested =
          SystemState::Get()->device_policy() &&
          SystemState::Get()->device_policy()->IsEnterpriseEnrolled();
    }
  }

  TEST_AND_RETURN(HasOutputPipe());
  if (HasOutputPipe())
    SetOutputObject(install_plan_);
  install_plan_.Dump();

  // Send the deadline data (if any) to Chrome through a file. This is a pretty
  // hacky solution but should be OK for now.
  //
  // TODO(petkov): Re-architect this to avoid communication through a
  // file. Ideally, we would include this information in D-Bus's GetStatus
  // method and UpdateStatus signal. A potential issue is that update_engine may
  // be unresponsive during an update download.
  if (!deadline_file_.empty()) {
    if (payload_state->GetRollbackHappened()) {
      // Don't do forced update if rollback has happened since the last update
      // check where policy was present.
      LOG(INFO) << "Not forcing update because a rollback happened.";
      utils::WriteFile(deadline_file_.c_str(), nullptr, 0);
    } else {
      utils::WriteFile(deadline_file_.c_str(),
                       response.deadline.data(),
                       response.deadline.size());
    }
    chmod(deadline_file_.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  }

  // Check the generated install-plan with the Policy to confirm that
  // it can be applied at this time (or at all).
  UpdateManager* const update_manager = SystemState::Get()->update_manager();
  CHECK(update_manager);
  auto ec = ErrorCode::kSuccess;
  update_manager->PolicyRequest(
      &Policy::UpdateCanBeApplied, &ec, &install_plan_);
  completer.set_code(ec);

  const auto allowed_milestones = params->rollback_allowed_milestones();
  if (allowed_milestones > 0) {
    auto max_firmware_rollforward = numeric_limits<uint32_t>::max();
    auto max_kernel_rollforward = numeric_limits<uint32_t>::max();

    // Determine the version to update the max rollforward verified boot
    // value.
    OmahaResponse::RollbackKeyVersion version =
        response.past_rollback_key_version;

    // Determine the max rollforward values to be set in the TPM.
    max_firmware_rollforward = static_cast<uint32_t>(version.firmware_key)
                                   << 16 |
                               static_cast<uint32_t>(version.firmware);
    max_kernel_rollforward = static_cast<uint32_t>(version.kernel_key) << 16 |
                             static_cast<uint32_t>(version.kernel);

    // In the case that the value is 0xffffffff, log a warning because the
    // device should not be installing a rollback image without having version
    // information.
    if (max_firmware_rollforward == numeric_limits<uint32_t>::max() ||
        max_kernel_rollforward == numeric_limits<uint32_t>::max()) {
      LOG(WARNING)
          << "Max rollforward values were not sent in rollback response: "
          << " max_kernel_rollforward=" << max_kernel_rollforward
          << " max_firmware_rollforward=" << max_firmware_rollforward
          << " rollback_allowed_milestones="
          << params->rollback_allowed_milestones();
    } else {
      LOG(INFO) << "Setting the max rollforward values: "
                << " max_kernel_rollforward=" << max_kernel_rollforward
                << " max_firmware_rollforward=" << max_firmware_rollforward
                << " rollback_allowed_milestones="
                << params->rollback_allowed_milestones();
      SystemState::Get()->hardware()->SetMaxKernelKeyRollforward(
          max_kernel_rollforward);
      // TODO(crbug/783998): Set max firmware rollforward when implemented.
    }
  } else {
    LOG(INFO) << "Rollback is not allowed. Setting max rollforward values"
              << " to infinity";
    // When rollback is not allowed, explicitly set the max roll forward to
    // infinity.
    SystemState::Get()->hardware()->SetMaxKernelKeyRollforward(
        kRollforwardInfinity);
    // TODO(crbug/783998): Set max firmware rollforward when implemented.
  }
}

bool OmahaResponseHandlerAction::AreHashChecksMandatory(
    const OmahaResponse& response) {
  // We sometimes need to waive the hash checks in order to download from
  // sources that don't provide hashes, such as dev server.
  // At this point UpdateAttempter::IsAnyUpdateSourceAllowed() has already been
  // checked, so an unofficial update URL won't get this far unless it's OK to
  // use without a hash. Additionally, we want to always waive hash checks on
  // unofficial builds (i.e. dev/test images).
  // The end result is this:
  //  * Base image:
  //    - Official URLs require a hash.
  //    - Unofficial URLs only get this far if the IsAnyUpdateSourceAllowed()
  //      devmode/debugd checks pass, in which case the hash is waived.
  //  * Dev/test image:
  //    - Any URL is allowed through with no hash checking.
  if (!SystemState::Get()->request_params()->IsUpdateUrlOfficial() ||
      !SystemState::Get()->hardware()->IsOfficialBuild()) {
    // Still do a hash check if a public key is included.
    if (!response.public_key_rsa.empty()) {
      // The autoupdate_CatchBadSignatures test checks for this string
      // in log-files. Keep in sync.
      LOG(INFO) << "Mandating payload hash checks since Omaha Response "
                << "for unofficial build includes public RSA key.";
      return true;
    } else {
      LOG(INFO) << "Waiving payload hash checks for unofficial update URL.";
      return false;
    }
  }

  LOG(INFO) << "Mandating hash checks for official URL on official build.";
  return true;
}

}  // namespace chromeos_update_engine
