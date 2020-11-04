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

#ifndef UPDATE_ENGINE_COMMON_MOCK_APP_EXCLUDER_H_
#define UPDATE_ENGINE_COMMON_MOCK_APP_EXCLUDER_H_

#include "update_engine/common/excluder_interface.h"

#include <string>

#include <gmock/gmock.h>

namespace chromeos_update_engine {

class MockExcluder : public ExcluderInterface {
 public:
  MOCK_METHOD(bool, Exclude, (const std::string&), (override));
  MOCK_METHOD(bool, IsExcluded, (const std::string&), (override));
  MOCK_METHOD(bool, Reset, (), (override));
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_COMMON_MOCK_APP_EXCLUDER_H_
