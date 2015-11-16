//
// Copyright (C) 2015 The Android Open Source Project
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

#include "update_engine/binder_service.h"

using android::OK;
using android::String16;
using android::os::IUpdateEnginePayloadApplicationCallback;
using android::sp;
using android::status_t;
using std::vector;

namespace chromeos_update_engine {

status_t BinderService::applyPayload(
    const String16& url,
    const vector<String16>& header_kv_pairs,
    const sp<IUpdateEnginePayloadApplicationCallback>& callback,
    int32_t* return_value) {
  *return_value = 0;
  return OK;
}

status_t BinderService::suspend() {
  return OK;
}

status_t BinderService::resume() {
  return OK;
}

status_t BinderService::cancel() {
  return OK;
}

}  // namespace chromeos_update_engine
