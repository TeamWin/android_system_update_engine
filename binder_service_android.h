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

#ifndef UPDATE_ENGINE_BINDER_SERVICE_ANDROID_H_
#define UPDATE_ENGINE_BINDER_SERVICE_ANDROID_H_

#include <vector>

#include <utils/Errors.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>

#include "android/os/BnUpdateEngine.h"
#include "android/os/IUpdateEngineCallback.h"
#include "update_engine/daemon_state_android.h"
#include "update_engine/service_observer_interface.h"

namespace chromeos_update_engine {

class BinderUpdateEngineAndroidService : public android::os::BnUpdateEngine,
                                         public ServiceObserverInterface {
 public:
  BinderUpdateEngineAndroidService(DaemonStateAndroid* daemon_state);
  ~BinderUpdateEngineAndroidService() override = default;

  const char* ServiceName() const {
    return "android.os.UpdateEngineService";
  }

  // ServiceObserverInterface overrides.
  void SendStatusUpdate(int64_t last_checked_time,
                        double progress,
                        update_engine::UpdateStatus status,
                        const std::string& new_version,
                        int64_t new_size) override;

  // Channel tracking changes are ignored.
  void SendChannelChangeUpdate(const std::string& tracking_channel) override {}

  // android::os::BnUpdateEngine overrides.
  android::binder::Status applyPayload(
      const android::String16& url,
      const std::vector<android::String16>& header_kv_pairs) override;

  android::binder::Status bind(
      const android::sp<android::os::IUpdateEngineCallback>& callback,
      bool* return_value) override;

  android::binder::Status suspend() override;

  android::binder::Status resume() override;

  android::binder::Status cancel() override;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_BINDER_SERVICE_ANDROID_H_
