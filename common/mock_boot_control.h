//
// Copyright (C) 2021 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_COMMON_MOCK_BOOT_CONTROL_H_
#define UPDATE_ENGINE_COMMON_MOCK_BOOT_CONTROL_H_

#include <memory>
#include <string>

#include <gmock/gmock.h>

#include "update_engine/common/boot_control_stub.h"

namespace chromeos_update_engine {

class MockBootControl final : public BootControlStub {
 public:
  MOCK_METHOD(bool,
              IsSlotMarkedSuccessful,
              (BootControlInterface::Slot),
              (const override));
  MOCK_METHOD(unsigned int, GetNumSlots, (), (const override));
  MOCK_METHOD(BootControlInterface::Slot, GetCurrentSlot, (), (const override));
  MOCK_METHOD(bool,
              GetPartitionDevice,
              (const std::string&, Slot, bool, std::string*, bool*),
              (const override));
  MOCK_METHOD(bool,
              GetPartitionDevice,
              (const std::string&, BootControlInterface::Slot, std::string*),
              (const override));
  MOCK_METHOD(std::optional<PartitionDevice>,
              GetPartitionDevice,
              (const std::string&, uint32_t, uint32_t, bool),
              (const override));

  MOCK_METHOD(bool,
              IsSlotBootable,
              (BootControlInterface::Slot),
              (const override));
  MOCK_METHOD(bool,
              MarkSlotUnbootable,
              (BootControlInterface::Slot),
              (override));
  MOCK_METHOD(bool,
              SetActiveBootSlot,
              (BootControlInterface::Slot),
              (override));
  MOCK_METHOD(bool,
              MarkBootSuccessfulAsync,
              (base::Callback<void(bool)>),
              (override));
  MOCK_METHOD(DynamicPartitionControlInterface*,
              GetDynamicPartitionControl,
              (),
              (override));
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_COMMON_MOCK_BOOT_CONTROL_H_
