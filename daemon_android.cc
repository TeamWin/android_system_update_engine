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

#include "update_engine/daemon_android.h"

#include <sysexits.h>

#include <binderwrapper/binder_wrapper.h>

#include "update_engine/daemon_state_android.h"

using std::unique_ptr;

namespace chromeos_update_engine {

unique_ptr<DaemonBase> DaemonBase::CreateInstance() {
  return std::make_unique<DaemonAndroid>();
}

int DaemonAndroid::OnInit() {
  // Register the |subprocess_| singleton with this Daemon as the signal
  // handler.
  subprocess_.Init(this);

  int exit_code = brillo::Daemon::OnInit();
  if (exit_code != EX_OK)
    return exit_code;

  android::BinderWrapper::Create();
  binder_watcher_.Init();

  DaemonStateAndroid* daemon_state_android = new DaemonStateAndroid();
  daemon_state_.reset(daemon_state_android);
  LOG_IF(ERROR, !daemon_state_android->Initialize())
      << "Failed to initialize system state.";

  auto binder_wrapper = android::BinderWrapper::Get();

  // Create the Binder Service.
  binder_service_ = new BinderUpdateEngineAndroidService{
      daemon_state_android->service_delegate()};
  if (!binder_wrapper->RegisterService(binder_service_->ServiceName(),
                                       binder_service_)) {
    LOG(ERROR) << "Failed to register binder service.";
  }
  daemon_state_->AddObserver(binder_service_.get());

  // Create the stable binder service.
  stable_binder_service_ = new BinderUpdateEngineAndroidStableService{
      daemon_state_android->service_delegate()};
  if (!binder_wrapper->RegisterService(stable_binder_service_->ServiceName(),
                                       stable_binder_service_)) {
    LOG(ERROR) << "Failed to register stable binder service.";
  }
  daemon_state_->AddObserver(stable_binder_service_.get());

  daemon_state_->StartUpdater();
  return EX_OK;
}

}  // namespace chromeos_update_engine
