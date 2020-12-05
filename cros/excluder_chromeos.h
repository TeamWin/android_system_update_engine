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

#ifndef UPDATE_ENGINE_CROS_EXCLUDER_CHROMEOS_H_
#define UPDATE_ENGINE_CROS_EXCLUDER_CHROMEOS_H_

#include <string>

#include "update_engine/common/excluder_interface.h"
#include "update_engine/common/prefs_interface.h"

namespace chromeos_update_engine {

class SystemState;

// The Chrome OS implementation of the |ExcluderInterface|.
class ExcluderChromeOS : public ExcluderInterface {
 public:
  ExcluderChromeOS() = default;
  ~ExcluderChromeOS() = default;

  // |ExcluderInterface| overrides.
  bool Exclude(const std::string& name) override;
  bool IsExcluded(const std::string& name) override;
  bool Reset() override;

  // Not copyable or movable.
  ExcluderChromeOS(const ExcluderChromeOS&) = delete;
  ExcluderChromeOS& operator=(const ExcluderChromeOS&) = delete;
  ExcluderChromeOS(ExcluderChromeOS&&) = delete;
  ExcluderChromeOS& operator=(ExcluderChromeOS&&) = delete;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CROS_EXCLUDER_CHROMEOS_H_
