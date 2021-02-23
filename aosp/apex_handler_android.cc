//
// Copyright (C) 2021 The Android Open Source Project
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

#include <utility>

#include <base/files/file_util.h>

#include "update_engine/aosp/apex_handler_android.h"
#include "update_engine/common/utils.h"

namespace chromeos_update_engine {

// Don't change this path... apexd relies on it.
constexpr const char* kApexReserveSpaceDir = "/data/apex/ota_reserved";

uint64_t ApexHandlerAndroid::CalculateSize(
    const std::vector<ApexInfo>& apex_infos) const {
  return CalculateSize(apex_infos, GetApexService());
}

uint64_t ApexHandlerAndroid::CalculateSize(
    const std::vector<ApexInfo>& apex_infos,
    android::sp<android::apex::IApexService> apex_service) const {
  // The safest option is to allocate space for every compressed APEX
  uint64_t size_required_default = 0;

  // We might not need to decompress every APEX. Communicate with apexd to get
  // accurate requirement.
  int64_t size_from_apexd;
  android::apex::CompressedApexInfoList compressed_apex_info_list;

  for (const auto& apex_info : apex_infos) {
    if (!apex_info.is_compressed()) {
      continue;
    }

    size_required_default += apex_info.decompressed_size();

    android::apex::CompressedApexInfo compressed_apex_info;
    compressed_apex_info.moduleName = apex_info.package_name();
    compressed_apex_info.versionCode = apex_info.version();
    compressed_apex_info.decompressedSize = apex_info.decompressed_size();
    compressed_apex_info_list.apexInfos.emplace_back(
        std::move(compressed_apex_info));
  }
  if (size_required_default == 0 || apex_service == nullptr) {
    return size_required_default;
  }

  auto result = apex_service->calculateSizeForCompressedApex(
      compressed_apex_info_list, &size_from_apexd);
  if (!result.isOk()) {
    return size_required_default;
  }
  return size_from_apexd;
}

bool ApexHandlerAndroid::AllocateSpace(const uint64_t size_required) const {
  return AllocateSpace(size_required, kApexReserveSpaceDir);
}

bool ApexHandlerAndroid::AllocateSpace(const uint64_t size_required,
                                       const std::string& dir_path) const {
  if (size_required == 0) {
    return true;
  }
  base::FilePath path{dir_path};
  // The filename is not important, it just needs to be under
  // kApexReserveSpaceDir. We call it "full.tmp" because the current space
  // estimation is simply adding up all decompressed sizes.
  path = path.Append("full.tmp");
  return utils::ReserveStorageSpace(path.value().c_str(), size_required);
}

android::sp<android::apex::IApexService> ApexHandlerAndroid::GetApexService()
    const {
  auto binder = android::defaultServiceManager()->waitForService(
      android::String16("apexservice"));
  if (binder == nullptr) {
    return nullptr;
  }
  return android::interface_cast<android::apex::IApexService>(binder);
}

}  // namespace chromeos_update_engine
