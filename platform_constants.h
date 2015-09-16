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

#ifndef UPDATE_ENGINE_PLATFORM_CONSTANTS_H_
#define UPDATE_ENGINE_PLATFORM_CONSTANTS_H_

namespace chromeos_update_engine {
namespace constants {

// The default URL used by all products when running in normal mode. The AUTest
// URL is used when testing normal images against the alternative AUTest server.
// Note that the URL can be override in run-time in certain cases.
extern const char kOmahaDefaultProductionURL[];
extern const char kOmahaDefaultAUTestURL[];

// Our product name used in Omaha. This value must match the one configured in
// the server side and is sent on every request.
extern const char kOmahaUpdaterID[];

// The name of the platform as sent to Omaha.
extern const char kOmahaPlatformName[];

}  // namespace constants
}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PLATFORM_CONSTANTS_H_
