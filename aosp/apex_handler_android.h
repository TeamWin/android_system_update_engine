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

#ifndef SYSTEM_UPDATE_ENGINE_AOSP_APEX_HANDLER_ANDROID_H_
#define SYSTEM_UPDATE_ENGINE_AOSP_APEX_HANDLER_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include <android/apex/IApexService.h>
#include <binder/IServiceManager.h>

#include "update_engine/aosp/apex_handler_interface.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

std::unique_ptr<ApexHandlerInterface> CreateApexHandler();

class ApexHandlerAndroid : virtual public ApexHandlerInterface {
 public:
  android::base::Result<uint64_t> CalculateSize(
      const std::vector<ApexInfo>& apex_infos) const;
  bool AllocateSpace(const std::vector<ApexInfo>& apex_infos) const;

 private:
  android::sp<android::apex::IApexService> GetApexService() const;
};

class FlattenedApexHandlerAndroid : virtual public ApexHandlerInterface {
 public:
  android::base::Result<uint64_t> CalculateSize(
      const std::vector<ApexInfo>& apex_infos) const;
  bool AllocateSpace(const std::vector<ApexInfo>& apex_infos) const;
};

}  // namespace chromeos_update_engine

#endif  // SYSTEM_UPDATE_ENGINE_AOSP_APEX_HANDLER_ANDROID_H_
