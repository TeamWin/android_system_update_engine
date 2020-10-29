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

#include "update_engine/requisition_util.h"

#include <memory>
#include <vector>

#include <base/files/file_util.h>
#include <base/json/json_file_value_serializer.h>
#include <base/logging.h>
#include <base/strings/string_util.h>

#include "update_engine/common/subprocess.h"
#include "update_engine/common/utils.h"

using std::string;
using std::vector;

namespace {

constexpr char kOemRequisitionKey[] = "oem_device_requisition";

}  // namespace

namespace chromeos_update_engine {

string ReadDeviceRequisition(const base::FilePath& local_state) {
  string requisition;
  bool vpd_retval = utils::GetVpdValue(kOemRequisitionKey, &requisition);

  // Some users manually convert non-CfM hardware at enrollment time, so VPD
  // value may be missing. So check the Local State JSON as well.
  if ((requisition.empty() || !vpd_retval) && base::PathExists(local_state)) {
    int error_code;
    std::string error_msg;
    JSONFileValueDeserializer deserializer(local_state);
    std::unique_ptr<base::Value> root =
        deserializer.Deserialize(&error_code, &error_msg);
    if (!root) {
      if (error_code != 0) {
        LOG(ERROR) << "Unable to deserialize Local State with exit code: "
                   << error_code << " and error: " << error_msg;
      }
      return "";
    }
    auto* path = root->FindPath({"enrollment", "device_requisition"});
    if (!path || !path->is_string()) {
      return "";
    }
    path->GetAsString(&requisition);
  }
  return requisition;
}

}  // namespace chromeos_update_engine
