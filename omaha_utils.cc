//
// Copyright (C) 2016 The Android Open Source Project
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

#include "update_engine/omaha_utils.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

namespace chromeos_update_engine {

const char kEolStatusSupported[] = "supported";
const char kEolStatusSecurityOnly[] = "security-only";
const char kEolStatusEol[] = "eol";

const EolDate kEolDateInvalid = -9999;

const char* EolStatusToString(EolStatus eol_status) {
  switch (eol_status) {
    case EolStatus::kSupported:
      return kEolStatusSupported;
    case EolStatus::kSecurityOnly:
      return kEolStatusSecurityOnly;
    case EolStatus::kEol:
      return kEolStatusEol;
  }
  // Only reached if an invalid number is casted to |EolStatus|.
  LOG(WARNING) << "Invalid EolStatus value: " << static_cast<int>(eol_status);
  return kEolStatusSupported;
}

EolStatus StringToEolStatus(const std::string& eol_status) {
  if (eol_status == kEolStatusSupported || eol_status.empty())
    return EolStatus::kSupported;
  if (eol_status == kEolStatusSecurityOnly)
    return EolStatus::kSecurityOnly;
  if (eol_status == kEolStatusEol)
    return EolStatus::kEol;
  LOG(WARNING) << "Invalid end-of-life attribute: " << eol_status;
  return EolStatus::kSupported;
}

std::string EolDateToString(EolDate eol_date) {
#if BASE_VER < 576279
  return base::Int64ToString(eol_date);
#else
  return base::NumberToString(eol_date);
#endif
}

EolDate StringToEolDate(const std::string& eol_date) {
  EolDate date = kEolDateInvalid;
  if (!base::StringToInt64(eol_date, &date)) {
    LOG(WARNING) << "Invalid EOL date attribute: " << eol_date;
    return kEolDateInvalid;
  }
  return date;
}

}  // namespace chromeos_update_engine
