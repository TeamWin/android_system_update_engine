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

#ifndef UPDATE_ENGINE_AOSP_DAEMON_ANDROID_H_
#define UPDATE_ENGINE_AOSP_DAEMON_ANDROID_H_

#include <memory>

#include <brillo/binder_watcher.h>

#include "update_engine/aosp/binder_service_android.h"
#include "update_engine/aosp/binder_service_stable_android.h"
#include "update_engine/common/daemon_base.h"
#include "update_engine/common/daemon_state_interface.h"
#include "update_engine/common/subprocess.h"

namespace chromeos_update_engine {

class DaemonAndroid : public DaemonBase {
 public:
  DaemonAndroid() = default;

 protected:
  int OnInit() override;

 private:
  // The Subprocess singleton class requires a |brillo::MessageLoop| in the
  // current thread, so we need to initialize it from this class instead of
  // the main() function.
  Subprocess subprocess_;

  brillo::BinderWatcher binder_watcher_;
  android::sp<BinderUpdateEngineAndroidService> binder_service_;
  android::sp<BinderUpdateEngineAndroidStableService> stable_binder_service_;

  // The daemon state with all the required daemon classes for the configured
  // platform.
  std::unique_ptr<DaemonStateInterface> daemon_state_;

  DISALLOW_COPY_AND_ASSIGN(DaemonAndroid);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_AOSP_DAEMON_ANDROID_H_
