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

#include "update_engine/binder_service_android.h"

using android::String16;
using android::binder::Status;
using android::os::IUpdateEngineCallback;
using android::sp;
using std::vector;

namespace chromeos_update_engine {

Status BinderUpdateEngineAndroidService::bind(
    const sp<IUpdateEngineCallback>& callback,
    bool* return_value) {
  *return_value = true;
  return Status::ok();
}

Status BinderUpdateEngineAndroidService::applyPayload(
    const String16& url,
    const vector<String16>& header_kv_pairs) {
  return Status::ok();
}

Status BinderUpdateEngineAndroidService::suspend() {
  return Status::ok();
}

Status BinderUpdateEngineAndroidService::resume() {
  return Status::ok();
}

Status BinderUpdateEngineAndroidService::cancel() {
  return Status::ok();
}

}  // namespace chromeos_update_engine
