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

#ifndef UPDATE_ENGINE_OMAHA_UTILS_H_
#define UPDATE_ENGINE_OMAHA_UTILS_H_

#include <string>

namespace chromeos_update_engine {

using EolDate = int64_t;

// |EolDate| indicating an invalid end-of-life date.
extern const EolDate kEolDateInvalid;

// Returns the string representation of the |eol_date|.
std::string EolDateToString(EolDate eol_date);

// Converts the end-of-life date string to an EolDate numeric value. In case
// of an invalid string, the default |kEolDateInvalid| value will be used
// instead.
EolDate StringToEolDate(const std::string& eol_date);

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_OMAHA_UTILS_H_
