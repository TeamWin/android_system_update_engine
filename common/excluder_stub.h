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

#ifndef UPDATE_ENGINE_COMMON_EXCLUDER_STUB_H_
#define UPDATE_ENGINE_COMMON_EXCLUDER_STUB_H_

#include <string>

#include "update_engine/common/excluder_interface.h"

namespace chromeos_update_engine {

// An implementation of the |ExcluderInterface| that does nothing.
class ExcluderStub : public ExcluderInterface {
 public:
  ExcluderStub() = default;
  ~ExcluderStub() = default;

  // |ExcluderInterface| overrides.
  bool Exclude(const std::string& name) override;
  bool IsExcluded(const std::string& name) override;
  bool Reset() override;

  // Not copyable or movable.
  ExcluderStub(const ExcluderStub&) = delete;
  ExcluderStub& operator=(const ExcluderStub&) = delete;
  ExcluderStub(ExcluderStub&&) = delete;
  ExcluderStub& operator=(ExcluderStub&&) = delete;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_COMMON_EXCLUDER_STUB_H_
