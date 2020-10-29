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

#ifndef UPDATE_ENGINE_AOSP_BINDER_SERVICE_ANDROID_COMMON_H_
#define UPDATE_ENGINE_AOSP_BINDER_SERVICE_ANDROID_COMMON_H_

#include <string>
#include <vector>

#include <binder/Status.h>

namespace chromeos_update_engine {

static inline android::binder::Status ErrorPtrToStatus(
    const brillo::ErrorPtr& error) {
  return android::binder::Status::fromServiceSpecificError(
      1, android::String8{error->GetMessage().c_str()});
}

static inline std::vector<std::string> ToVecString(
    const std::vector<android::String16>& inp) {
  std::vector<std::string> out;
  out.reserve(inp.size());
  for (const auto& e : inp) {
    out.emplace_back(android::String8{e}.string());
  }
  return out;
}

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_AOSP_BINDER_SERVICE_ANDROID_COMMON_H_
