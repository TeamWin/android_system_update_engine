//
// Copyright (C) 2018 The Android Open Source Project
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

#include "update_engine/dlcservice_chromeos.h"

#include <brillo/errors/error.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>
// NOLINTNEXTLINE(build/include_alpha) "dbus-proxies.h" needs "dlcservice.pb.h"
#include <dlcservice/dbus-proxies.h>

#include "update_engine/dbus_connection.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
org::chromium::DlcServiceInterfaceProxy GetDlcServiceProxy() {
  return {DBusConnection::Get()->GetDBus()};
}
}  // namespace

std::unique_ptr<DlcServiceInterface> CreateDlcService() {
  return std::make_unique<DlcServiceChromeOS>();
}

bool DlcServiceChromeOS::GetDlcsToUpdate(vector<string>* dlc_ids) {
  if (!dlc_ids)
    return false;
  dlc_ids->clear();

  brillo::ErrorPtr err;
  if (!GetDlcServiceProxy().GetDlcsToUpdate(dlc_ids, &err)) {
    LOG(ERROR) << "dlcservice failed to return DLCs that need to be updated. "
               << "ErrorCode=" << err->GetCode()
               << ", ErrMsg=" << err->GetMessage();
    dlc_ids->clear();
    return false;
  }
  return true;
}

bool DlcServiceChromeOS::InstallCompleted(const vector<string>& dlc_ids) {
  brillo::ErrorPtr err;
  if (!GetDlcServiceProxy().InstallCompleted(dlc_ids, &err)) {
    LOG(ERROR) << "dlcservice failed to complete install. ErrCode="
               << err->GetCode() << ", ErrMsg=" << err->GetMessage();
    return false;
  }
  return true;
}

bool DlcServiceChromeOS::UpdateCompleted(const vector<string>& dlc_ids) {
  brillo::ErrorPtr err;
  if (!GetDlcServiceProxy().UpdateCompleted(dlc_ids, &err)) {
    LOG(ERROR) << "dlcservice failed to complete updated. ErrCode="
               << err->GetCode() << ", ErrMsg=" << err->GetMessage();
    return false;
  }
  return true;
}

}  // namespace chromeos_update_engine
