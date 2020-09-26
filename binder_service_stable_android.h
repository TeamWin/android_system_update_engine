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

#ifndef UPDATE_ENGINE_BINDER_SERVICE_STABLE_ANDROID_H_
#define UPDATE_ENGINE_BINDER_SERVICE_STABLE_ANDROID_H_

#include <stdint.h>

#include <string>
#include <vector>

#include <utils/Errors.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>

#include "android/os/BnUpdateEngineStable.h"
#include "android/os/IUpdateEngineStableCallback.h"
#include "update_engine/service_delegate_android_interface.h"
#include "update_engine/service_observer_interface.h"

namespace chromeos_update_engine {

class BinderUpdateEngineAndroidStableService
    : public android::os::BnUpdateEngineStable,
      public ServiceObserverInterface {
 public:
  explicit BinderUpdateEngineAndroidStableService(
      ServiceDelegateAndroidInterface* service_delegate);
  ~BinderUpdateEngineAndroidStableService() override = default;

  const char* ServiceName() const {
    return "android.os.UpdateEngineStableService";
  }

  // ServiceObserverInterface overrides.
  void SendStatusUpdate(
      const update_engine::UpdateEngineStatus& update_engine_status) override;
  void SendPayloadApplicationComplete(ErrorCode error_code) override;

  // android::os::BnUpdateEngineStable overrides.
  android::binder::Status applyPayloadFd(
      const ::android::os::ParcelFileDescriptor& pfd,
      int64_t payload_offset,
      int64_t payload_size,
      const std::vector<android::String16>& header_kv_pairs) override;
  android::binder::Status bind(
      const android::sp<android::os::IUpdateEngineStableCallback>& callback,
      bool* return_value) override;
  android::binder::Status unbind(
      const android::sp<android::os::IUpdateEngineStableCallback>& callback,
      bool* return_value) override;

 private:
  // Remove the passed |callback| from the list of registered callbacks. Called
  // on unbind() or whenever the callback object is destroyed.
  // Returns true on success.
  bool UnbindCallback(const IBinder* callback);

  // Bound callback. The stable interface only supports one callback at a time.
  android::sp<android::os::IUpdateEngineStableCallback> callback_;

  // Cached copy of the last status update sent. Used to send an initial
  // notification when bind() is called from the client.
  int last_status_{-1};
  double last_progress_{0.0};

  ServiceDelegateAndroidInterface* service_delegate_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_BINDER_SERVICE_STABLE_ANDROID_H_
