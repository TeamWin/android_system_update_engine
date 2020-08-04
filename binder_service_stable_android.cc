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

#include "update_engine/binder_service_stable_android.h"

#include <memory>

#include <base/bind.h>
#include <base/logging.h>
#include <binderwrapper/binder_wrapper.h>
#include <brillo/errors/error.h>
#include <utils/String8.h>

#include "update_engine/binder_service_android_common.h"

using android::binder::Status;
using android::os::IUpdateEngineStableCallback;
using android::os::ParcelFileDescriptor;
using std::string;
using std::vector;
using update_engine::UpdateEngineStatus;

namespace chromeos_update_engine {

BinderUpdateEngineAndroidStableService::BinderUpdateEngineAndroidStableService(
    ServiceDelegateAndroidInterface* service_delegate)
    : service_delegate_(service_delegate) {}

void BinderUpdateEngineAndroidStableService::SendStatusUpdate(
    const UpdateEngineStatus& update_engine_status) {
  last_status_ = static_cast<int>(update_engine_status.status);
  last_progress_ = update_engine_status.progress;
  if (callback_) {
    callback_->onStatusUpdate(last_status_, last_progress_);
  }
}

void BinderUpdateEngineAndroidStableService::SendPayloadApplicationComplete(
    ErrorCode error_code) {
  if (callback_) {
    callback_->onPayloadApplicationComplete(static_cast<int>(error_code));
  }
}

Status BinderUpdateEngineAndroidStableService::bind(
    const android::sp<IUpdateEngineStableCallback>& callback,
    bool* return_value) {
  // Reject binding if another callback is already bound.
  if (callback_ != nullptr) {
    LOG(ERROR) << "Another callback is already bound. Can't bind new callback.";
    *return_value = false;
    return Status::ok();
  }

  // See BinderUpdateEngineAndroidService::bind.
  if (last_status_ != -1) {
    auto status = callback->onStatusUpdate(last_status_, last_progress_);
    if (!status.isOk()) {
      LOG(ERROR) << "Failed to call onStatusUpdate() from callback: "
                 << status.toString8();
      *return_value = false;
      return Status::ok();
    }
  }

  callback_ = callback;

  const android::sp<IBinder>& callback_binder =
      IUpdateEngineStableCallback::asBinder(callback);
  auto binder_wrapper = android::BinderWrapper::Get();
  binder_wrapper->RegisterForDeathNotifications(
      callback_binder,
      base::Bind(base::IgnoreResult(
                     &BinderUpdateEngineAndroidStableService::UnbindCallback),
                 base::Unretained(this),
                 base::Unretained(callback_binder.get())));

  *return_value = true;
  return Status::ok();
}

Status BinderUpdateEngineAndroidStableService::unbind(
    const android::sp<IUpdateEngineStableCallback>& callback,
    bool* return_value) {
  const android::sp<IBinder>& callback_binder =
      IUpdateEngineStableCallback::asBinder(callback);
  auto binder_wrapper = android::BinderWrapper::Get();
  binder_wrapper->UnregisterForDeathNotifications(callback_binder);

  *return_value = UnbindCallback(callback_binder.get());
  return Status::ok();
}

Status BinderUpdateEngineAndroidStableService::applyPayloadFd(
    const ParcelFileDescriptor& pfd,
    int64_t payload_offset,
    int64_t payload_size,
    const vector<android::String16>& header_kv_pairs) {
  vector<string> str_headers = ToVecString(header_kv_pairs);

  brillo::ErrorPtr error;
  if (!service_delegate_->ApplyPayload(
          pfd.get(), payload_offset, payload_size, str_headers, &error)) {
    return ErrorPtrToStatus(error);
  }
  return Status::ok();
}

bool BinderUpdateEngineAndroidStableService::UnbindCallback(
    const IBinder* callback) {
  if (IUpdateEngineStableCallback::asBinder(callback_).get() != callback) {
    LOG(ERROR) << "Unable to unbind unknown callback.";
    return false;
  }
  callback_ = nullptr;
  return true;
}

}  // namespace chromeos_update_engine
