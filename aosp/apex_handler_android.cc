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

namespace {

android::apex::CompressedApexInfoList CreateCompressedApexInfoList(
    const std::vector<ApexInfo>& apex_infos) {
  android::apex::CompressedApexInfoList compressed_apex_info_list;
  for (const auto& apex_info : apex_infos) {
    if (!apex_info.is_compressed()) {
      continue;
    }
    android::apex::CompressedApexInfo compressed_apex_info;
    compressed_apex_info.moduleName = apex_info.package_name();
    compressed_apex_info.versionCode = apex_info.version();
    compressed_apex_info.decompressedSize = apex_info.decompressed_size();
    compressed_apex_info_list.apexInfos.emplace_back(
        std::move(compressed_apex_info));
  }
  return compressed_apex_info_list;
}

}  // namespace

android::base::Result<uint64_t> ApexHandlerAndroid::CalculateSize(
    const std::vector<ApexInfo>& apex_infos) const {
  // We might not need to decompress every APEX. Communicate with apexd to get
  // accurate requirement.
  auto apex_service = GetApexService();
  if (apex_service == nullptr) {
    return android::base::Error() << "Failed to get hold of apexservice";
  }

  auto compressed_apex_info_list = CreateCompressedApexInfoList(apex_infos);
  int64_t size_from_apexd;
  auto result = apex_service->calculateSizeForCompressedApex(
      compressed_apex_info_list, &size_from_apexd);
  if (!result.isOk()) {
    return android::base::Error()
           << "Failed to get size required from apexservice";
  }
  return size_from_apexd;
}

bool ApexHandlerAndroid::AllocateSpace(
    const std::vector<ApexInfo>& apex_infos) const {
  auto apex_service = GetApexService();
  if (apex_service == nullptr) {
    return false;
  }
  auto compressed_apex_info_list = CreateCompressedApexInfoList(apex_infos);
  auto result =
      apex_service->reserveSpaceForCompressedApex(compressed_apex_info_list);
  return result.isOk();
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
