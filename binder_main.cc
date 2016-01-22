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

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/Looper.h>
#include <utils/StrongPointer.h>

#include "update_engine/binder_service_android.h"

// Log to logcat as update_engine.
#undef LOG_TAG
#define LOG_TAG "update_engine"

namespace android {
namespace {

class BinderCallback : public LooperCallback {
 public:
  BinderCallback() {}
  ~BinderCallback() override {}

  int handleEvent(int /* fd */, int /* events */, void* /* data */) override {
    IPCThreadState::self()->handlePolledCommands();
    return 1;  // Continue receiving callbacks.
  }
};

bool run(const sp<IBinder>& service) {
  sp<Looper> looper(Looper::prepare(0 /* opts */));

  ALOGD("Connecting to binder driver");
  int binder_fd = -1;
  ProcessState::self()->setThreadPoolMaxThreadCount(0);
  IPCThreadState::self()->disableBackgroundScheduling(true);
  IPCThreadState::self()->setupPolling(&binder_fd);
  if (binder_fd < 0) {
    return false;
  }

  sp<BinderCallback> cb(new BinderCallback);
  if (looper->addFd(binder_fd, Looper::POLL_CALLBACK, Looper::EVENT_INPUT, cb,
                    nullptr) != 1) {
    ALOGE("Failed to add binder FD to Looper");
    return false;
  }

  ALOGD("Registering update_engine with the service manager");
  status_t status = defaultServiceManager()->addService(
      service->getInterfaceDescriptor(), service);
  if (status != android::OK) {
    ALOGE("Failed to register update_engine with the service manager.");
    return false;
  }

  ALOGD("Entering update_engine mainloop");
  while (true) {
    const int result = looper->pollAll(-1 /* timeoutMillis */);
    ALOGD("Looper returned %d", result);
  }
  // We should never get here.
  return false;
}

}  // namespace
}  // namespace android

int main(int argc, char** argv) {
  android::sp<android::IBinder> service(
      new chromeos_update_engine::BinderService);
  if (!android::run(service)) {
    return 1;
  }
  return 0;
}
