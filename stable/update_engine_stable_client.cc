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

// update_engine console client installed to APEXes for scripts to invoke
// directly. Uses the stable API.

#include <fcntl.h>
#include <sysexits.h>
#include <unistd.h>

#include <vector>

#include <aidl/android/os/BnUpdateEngineStableCallback.h>
#include <aidl/android/os/IUpdateEngineStable.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_ibinder.h>
#include <common/error_code.h>
#include <gflags/gflags.h>
#include <utils/StrongPointer.h>

namespace chromeos_update_engine::internal {

DEFINE_string(payload,
              "file:///path/to/payload.bin",
              "The file URI to the update payload to use, or path to the file");
DEFINE_int64(offset,
             0,
             "The offset in the payload where the CrAU update starts.");
DEFINE_int64(size,
             0,
             "The size of the CrAU part of the payload. If 0 is passed, it "
             "will be autodetected.");
DEFINE_string(headers,
              "",
              "A list of key-value pairs, one element of the list per line.");

[[noreturn]] int Exit(int return_code) {
  LOG(INFO) << "Exit: " << return_code;
  exit(return_code);
}
// Called whenever the UpdateEngine daemon dies.
void UpdateEngineServiceDied(void*) {
  LOG(ERROR) << "UpdateEngineService died.";
  Exit(EX_SOFTWARE);
}

class UpdateEngineClientAndroid {
 public:
  UpdateEngineClientAndroid() = default;
  int Run();

 private:
  class UECallback : public aidl::android::os::BnUpdateEngineStableCallback {
   public:
    UECallback() = default;

    // android::os::BnUpdateEngineStableCallback overrides.
    ndk::ScopedAStatus onStatusUpdate(int status_code, float progress) override;
    ndk::ScopedAStatus onPayloadApplicationComplete(int error_code) override;
  };

  static std::vector<std::string> ParseHeaders(const std::string& arg);

  const ndk::ScopedAIBinder_DeathRecipient death_recipient_{
      AIBinder_DeathRecipient_new(&UpdateEngineServiceDied)};
  std::shared_ptr<aidl::android::os::IUpdateEngineStable> service_;
  std::shared_ptr<aidl::android::os::BnUpdateEngineStableCallback> callback_;
};

ndk::ScopedAStatus UpdateEngineClientAndroid::UECallback::onStatusUpdate(
    int status_code, float progress) {
  LOG(INFO) << "onStatusUpdate(" << status_code << ", " << progress << ")";
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
UpdateEngineClientAndroid::UECallback::onPayloadApplicationComplete(
    int error_code) {
  LOG(INFO) << "onPayloadApplicationComplete(" << error_code << ")";
  auto code = static_cast<ErrorCode>(error_code);
  Exit((code == ErrorCode::kSuccess || code == ErrorCode::kUpdatedButNotActive)
           ? EX_OK
           : EX_SOFTWARE);
}

int UpdateEngineClientAndroid::Run() {
  service_ = aidl::android::os::IUpdateEngineStable::fromBinder(ndk::SpAIBinder(
      AServiceManager_getService("android.os.UpdateEngineStableService")));
  if (service_ == nullptr) {
    LOG(ERROR)
        << "Failed to get IUpdateEngineStable binder from service manager.";
    return EX_SOFTWARE;
  }

  // Register a callback object with the service.
  callback_ = ndk::SharedRefBase::make<UECallback>();
  bool bound;
  if (!service_->bind(callback_, &bound).isOk() || !bound) {
    LOG(ERROR) << "Failed to bind() the UpdateEngine daemon.";
    return EX_SOFTWARE;
  }

  auto headers = ParseHeaders(FLAGS_headers);
  ndk::ScopedAStatus status;
  const char* payload_path;
  std::string file_prefix = "file://";
  if (android::base::StartsWith(FLAGS_payload, file_prefix)) {
    payload_path = FLAGS_payload.data() + file_prefix.length();
  } else {
    payload_path = FLAGS_payload.data();
  }
  ndk::ScopedFileDescriptor ufd(
      TEMP_FAILURE_RETRY(open(payload_path, O_RDONLY)));
  if (ufd.get() < 0) {
    PLOG(ERROR) << "Can't open " << payload_path;
    return EX_SOFTWARE;
  }
  status = service_->applyPayloadFd(ufd, FLAGS_offset, FLAGS_size, headers);
  if (!status.isOk()) {
    LOG(ERROR) << "Cannot apply payload: " << status.getDescription();
    return EX_SOFTWARE;
  }

  // When following updates status changes, exit if the update_engine daemon
  // dies.
  if (AIBinder_linkToDeath(service_->asBinder().get(),
                           death_recipient_.get(),
                           nullptr) != STATUS_OK) {
    return EX_SOFTWARE;
  }

  return EX_OK;
}

std::vector<std::string> UpdateEngineClientAndroid::ParseHeaders(
    const std::string& arg) {
  std::vector<std::string> lines = android::base::Split(arg, "\n");
  std::vector<std::string> headers;
  for (const auto& line : lines) {
    auto header = android::base::Trim(line);
    if (!header.empty()) {
      headers.push_back(header);
    }
  }
  return headers;
}

}  // namespace chromeos_update_engine::internal

int main(int argc, char** argv) {
  android::base::InitLogging(argv);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // Unlike other update_engine* processes that uses message loops,
  // update_engine_stable_client uses a thread pool model. However, number of
  // threads is limited to 1; that is, 0 additional threads should be spawned.
  // This avoids some race conditions.
  if (!ABinderProcess_setThreadPoolMaxThreadCount(0)) {
    LOG(ERROR) << "Cannot set thread pool max thread count";
    return EX_SOFTWARE;
  }
  ABinderProcess_startThreadPool();

  chromeos_update_engine::internal::UpdateEngineClientAndroid client{};
  int code = client.Run();
  if (code != EX_OK)
    return code;

  ABinderProcess_joinThreadPool();
  LOG(ERROR) << "Exited from joinThreadPool.";
  return EX_SOFTWARE;
}
