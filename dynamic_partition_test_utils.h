//
// Copyright (C) 2019 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_DYNAMIC_PARTITION_TEST_UTILS_H_
#define UPDATE_ENGINE_DYNAMIC_PARTITION_TEST_UTILS_H_

#include <stdint.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/strings/string_util.h>
#include <fs_mgr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <liblp/builder.h>

#include "update_engine/common/boot_control_interface.h"

namespace chromeos_update_engine {

using android::fs_mgr::MetadataBuilder;
using testing::_;
using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;

constexpr const uint32_t kMaxNumSlots = 2;
constexpr const char* kSlotSuffixes[kMaxNumSlots] = {"_a", "_b"};
constexpr const char* kFakeDevicePath = "/fake/dev/path/";
constexpr const char* kFakeDmDevicePath = "/fake/dm/dev/path/";
constexpr const uint32_t kFakeMetadataSize = 65536;
constexpr const char* kDefaultGroup = "foo";
constexpr const char* kFakeSuper = "fake_super";

// A map describing the size of each partition.
// "{name, size}"
using PartitionSizes = std::map<std::string, uint64_t>;

// "{name_a, size}"
using PartitionSuffixSizes = std::map<std::string, uint64_t>;

using PartitionMetadata = BootControlInterface::PartitionMetadata;

// C++ standards do not allow uint64_t (aka unsigned long) to be the parameter
// of user-defined literal operators.
// clang-format off
inline constexpr unsigned long long operator"" _MiB(unsigned long long x) {  // NOLINT
  return x << 20;
}
inline constexpr unsigned long long operator"" _GiB(unsigned long long x) {  // NOLINT
  return x << 30;
}
// clang-format on

constexpr uint64_t kDefaultGroupSize = 5_GiB;
// Super device size. 1 MiB for metadata.
constexpr uint64_t kDefaultSuperSize = kDefaultGroupSize * 2 + 1_MiB;

template <typename U, typename V>
inline std::ostream& operator<<(std::ostream& os, const std::map<U, V>& param) {
  os << "{";
  bool first = true;
  for (const auto& pair : param) {
    if (!first)
      os << ", ";
    os << pair.first << ":" << pair.second;
    first = false;
  }
  return os << "}";
}

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& param) {
  os << "[";
  bool first = true;
  for (const auto& e : param) {
    if (!first)
      os << ", ";
    os << e;
    first = false;
  }
  return os << "]";
}

inline std::ostream& operator<<(std::ostream& os,
                                const PartitionMetadata::Partition& p) {
  return os << "{" << p.name << ", " << p.size << "}";
}

inline std::ostream& operator<<(std::ostream& os,
                                const PartitionMetadata::Group& g) {
  return os << "{" << g.name << ", " << g.size << ", " << g.partitions << "}";
}

inline std::ostream& operator<<(std::ostream& os, const PartitionMetadata& m) {
  return os << m.groups;
}

inline std::string GetDevice(const std::string& name) {
  return kFakeDevicePath + name;
}

inline std::string GetDmDevice(const std::string& name) {
  return kFakeDmDevicePath + name;
}

// To support legacy tests, auto-convert {name_a: size} map to
// PartitionMetadata.
inline PartitionMetadata PartitionSuffixSizesToMetadata(
    const PartitionSuffixSizes& partition_sizes) {
  PartitionMetadata metadata;
  for (const char* suffix : kSlotSuffixes) {
    metadata.groups.push_back(
        {std::string(kDefaultGroup) + suffix, kDefaultGroupSize, {}});
  }
  for (const auto& pair : partition_sizes) {
    for (size_t suffix_idx = 0; suffix_idx < kMaxNumSlots; ++suffix_idx) {
      if (base::EndsWith(pair.first,
                         kSlotSuffixes[suffix_idx],
                         base::CompareCase::SENSITIVE)) {
        metadata.groups[suffix_idx].partitions.push_back(
            {pair.first, pair.second});
      }
    }
  }
  return metadata;
}

// To support legacy tests, auto-convert {name: size} map to PartitionMetadata.
inline PartitionMetadata PartitionSizesToMetadata(
    const PartitionSizes& partition_sizes) {
  PartitionMetadata metadata;
  metadata.groups.push_back(
      {std::string{kDefaultGroup}, kDefaultGroupSize, {}});
  for (const auto& pair : partition_sizes) {
    metadata.groups[0].partitions.push_back({pair.first, pair.second});
  }
  return metadata;
}

inline std::unique_ptr<MetadataBuilder> NewFakeMetadata(
    const PartitionMetadata& metadata) {
  auto builder =
      MetadataBuilder::New(kDefaultSuperSize, kFakeMetadataSize, kMaxNumSlots);
  EXPECT_GE(builder->AllocatableSpace(), kDefaultGroupSize * 2);
  EXPECT_NE(nullptr, builder);
  if (builder == nullptr)
    return nullptr;
  for (const auto& group : metadata.groups) {
    EXPECT_TRUE(builder->AddGroup(group.name, group.size));
    for (const auto& partition : group.partitions) {
      auto p = builder->AddPartition(partition.name, group.name, 0 /* attr */);
      EXPECT_TRUE(p && builder->ResizePartition(p, partition.size));
    }
  }
  return builder;
}

class MetadataMatcher : public MatcherInterface<MetadataBuilder*> {
 public:
  explicit MetadataMatcher(const PartitionSuffixSizes& partition_sizes)
      : partition_metadata_(PartitionSuffixSizesToMetadata(partition_sizes)) {}
  explicit MetadataMatcher(const PartitionMetadata& partition_metadata)
      : partition_metadata_(partition_metadata) {}

  bool MatchAndExplain(MetadataBuilder* metadata,
                       MatchResultListener* listener) const override {
    bool success = true;
    for (const auto& group : partition_metadata_.groups) {
      for (const auto& partition : group.partitions) {
        auto p = metadata->FindPartition(partition.name);
        if (p == nullptr) {
          if (!success)
            *listener << "; ";
          *listener << "No partition " << partition.name;
          success = false;
          continue;
        }
        if (p->size() != partition.size) {
          if (!success)
            *listener << "; ";
          *listener << "Partition " << partition.name << " has size "
                    << p->size() << ", expected " << partition.size;
          success = false;
        }
        if (p->group_name() != group.name) {
          if (!success)
            *listener << "; ";
          *listener << "Partition " << partition.name << " has group "
                    << p->group_name() << ", expected " << group.name;
          success = false;
        }
      }
    }
    return success;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "expect: " << partition_metadata_;
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "expect not: " << partition_metadata_;
  }

 private:
  PartitionMetadata partition_metadata_;
};

inline Matcher<MetadataBuilder*> MetadataMatches(
    const PartitionSuffixSizes& partition_sizes) {
  return MakeMatcher(new MetadataMatcher(partition_sizes));
}

inline Matcher<MetadataBuilder*> MetadataMatches(
    const PartitionMetadata& partition_metadata) {
  return MakeMatcher(new MetadataMatcher(partition_metadata));
}

MATCHER_P(HasGroup, group, " has group " + group) {
  auto groups = arg->ListGroups();
  return std::find(groups.begin(), groups.end(), group) != groups.end();
}

struct TestParam {
  uint32_t source;
  uint32_t target;
};
inline std::ostream& operator<<(std::ostream& os, const TestParam& param) {
  return os << "{source: " << param.source << ", target:" << param.target
            << "}";
}

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DYNAMIC_PARTITION_TEST_UTILS_H_
