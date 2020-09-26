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

#include "update_engine/payload_consumer/partition_update_generator_android.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include <android-base/strings.h>
#include <brillo/secure_blob.h>
#include <gtest/gtest.h>

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/fake_boot_control.h"
#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"

namespace chromeos_update_engine {

class FakePartitionUpdateGenerator : public PartitionUpdateGeneratorAndroid {
 public:
  std::vector<std::string> GetAbPartitionsOnDevice() const {
    return ab_partitions_;
  }
  using PartitionUpdateGeneratorAndroid::PartitionUpdateGeneratorAndroid;
  std::vector<std::string> ab_partitions_;
};

class PartitionUpdateGeneratorAndroidTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(device_dir_.CreateUniqueTempDir());
    boot_control_ = std::make_unique<FakeBootControl>();
    ASSERT_TRUE(boot_control_);
    boot_control_->SetNumSlots(2);
    generator_ = std::make_unique<FakePartitionUpdateGenerator>(
        boot_control_.get(), 4096);
    ASSERT_TRUE(generator_);
  }

  std::unique_ptr<FakePartitionUpdateGenerator> generator_;
  std::unique_ptr<FakeBootControl> boot_control_;

  base::ScopedTempDir device_dir_;
  std::map<std::string, std::string> device_map_;

  void SetUpBlockDevice(const std::map<std::string, std::string>& contents) {
    std::set<std::string> partition_base_names;
    for (const auto& [name, content] : contents) {
      auto path = device_dir_.GetPath().value() + "/" + name;
      ASSERT_TRUE(
          utils::WriteFile(path.c_str(), content.data(), content.size()));

      if (android::base::EndsWith(name, "_a")) {
        auto prefix = name.substr(0, name.size() - 2);
        boot_control_->SetPartitionDevice(prefix, 0, path);
        partition_base_names.emplace(prefix);
      } else if (android::base::EndsWith(name, "_b")) {
        auto prefix = name.substr(0, name.size() - 2);
        boot_control_->SetPartitionDevice(prefix, 1, path);
        partition_base_names.emplace(prefix);
      }
      device_map_[name] = std::move(path);
    }
    generator_->ab_partitions_ = {partition_base_names.begin(),
                                  partition_base_names.end()};
  }

  void CheckPartitionUpdate(const std::string& name,
                            const std::string& content,
                            const PartitionUpdate& partition_update) {
    ASSERT_EQ(name, partition_update.partition_name());

    brillo::Blob out_hash;
    ASSERT_TRUE(HashCalculator::RawHashOfBytes(
        content.data(), content.size(), &out_hash));
    ASSERT_EQ(std::string(out_hash.begin(), out_hash.end()),
              partition_update.old_partition_info().hash());
    ASSERT_EQ(std::string(out_hash.begin(), out_hash.end()),
              partition_update.new_partition_info().hash());

    ASSERT_EQ(1, partition_update.operations_size());
    const auto& operation = partition_update.operations(0);
    ASSERT_EQ(InstallOperation::SOURCE_COPY, operation.type());

    ASSERT_EQ(1, operation.src_extents_size());
    ASSERT_EQ(0u, operation.src_extents(0).start_block());
    ASSERT_EQ(content.size() / 4096, operation.src_extents(0).num_blocks());

    ASSERT_EQ(1, operation.dst_extents_size());
    ASSERT_EQ(0u, operation.dst_extents(0).start_block());
    ASSERT_EQ(content.size() / 4096, operation.dst_extents(0).num_blocks());
  }
};

TEST_F(PartitionUpdateGeneratorAndroidTest, CreatePartitionUpdate) {
  auto system_contents = std::string(4096 * 2, '1');
  auto boot_contents = std::string(4096 * 5, 'b');
  std::map<std::string, std::string> contents = {
      {"system_a", system_contents},
      {"system_b", std::string(4096 * 2, 0)},
      {"boot_a", boot_contents},
      {"boot_b", std::string(4096 * 5, 0)},
  };
  SetUpBlockDevice(contents);

  auto system_partition_update = generator_->CreatePartitionUpdate(
      "system", device_map_["system_a"], device_map_["system_b"], 4096 * 2);
  ASSERT_TRUE(system_partition_update.has_value());
  CheckPartitionUpdate(
      "system", system_contents, system_partition_update.value());

  auto boot_partition_update = generator_->CreatePartitionUpdate(
      "boot", device_map_["boot_a"], device_map_["boot_b"], 4096 * 5);
  ASSERT_TRUE(boot_partition_update.has_value());
  CheckPartitionUpdate("boot", boot_contents, boot_partition_update.value());
}

TEST_F(PartitionUpdateGeneratorAndroidTest, GenerateOperations) {
  auto system_contents = std::string(4096 * 10, '2');
  auto boot_contents = std::string(4096 * 5, 'b');
  std::map<std::string, std::string> contents = {
      {"system_a", system_contents},
      {"system_b", std::string(4096 * 10, 0)},
      {"boot_a", boot_contents},
      {"boot_b", std::string(4096 * 5, 0)},
      {"vendor_a", ""},
      {"vendor_b", ""},
      {"persist", ""},
  };
  SetUpBlockDevice(contents);

  std::vector<PartitionUpdate> update_list;
  ASSERT_TRUE(generator_->GenerateOperationsForPartitionsNotInPayload(
      0, 1, std::set<std::string>{"vendor"}, &update_list));

  ASSERT_EQ(2u, update_list.size());
  CheckPartitionUpdate("boot", boot_contents, update_list[0]);
  CheckPartitionUpdate("system", system_contents, update_list[1]);
}

}  // namespace chromeos_update_engine
