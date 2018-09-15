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

#ifndef UPDATE_ENGINE_PAYLOAD_CONSUMER_VERITY_WRITER_ANDROID_H_
#define UPDATE_ENGINE_PAYLOAD_CONSUMER_VERITY_WRITER_ANDROID_H_

#include <memory>

#include <verity/hash_tree_builder.h>

#include "update_engine/payload_consumer/verity_writer_interface.h"

namespace chromeos_update_engine {

class VerityWriterAndroid : public VerityWriterInterface {
 public:
  VerityWriterAndroid() = default;
  ~VerityWriterAndroid() override = default;

  bool Init(const InstallPlan::Partition& partition) override;
  bool Update(uint64_t offset, const uint8_t* buffer, size_t size) override;

 private:
  const InstallPlan::Partition* partition_ = nullptr;

  std::unique_ptr<HashTreeBuilder> hash_tree_builder_;

  DISALLOW_COPY_AND_ASSIGN(VerityWriterAndroid);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_CONSUMER_VERITY_WRITER_ANDROID_H_
