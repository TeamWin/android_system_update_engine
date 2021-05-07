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

#include "update_engine/payload_consumer/cow_writer_file_descriptor.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <android-base/unique_fd.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libsnapshot/snapshot_writer.h>

#include "update_engine/common/utils.h"

namespace chromeos_update_engine {
constexpr size_t BLOCK_SIZE = 4096;
constexpr size_t PARTITION_SIZE = BLOCK_SIZE * 10;

using android::base::unique_fd;
using android::snapshot::CompressedSnapshotWriter;
using android::snapshot::CowOptions;
using android::snapshot::ISnapshotWriter;

class CowWriterFileDescriptorUnittest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_EQ(ftruncate64(cow_device_file_.fd(), PARTITION_SIZE), 0)
        << "Failed to truncate cow_device file to " << PARTITION_SIZE
        << strerror(errno);
    ASSERT_EQ(ftruncate64(cow_source_file_.fd(), PARTITION_SIZE), 0)
        << "Failed to truncate cow_source file to " << PARTITION_SIZE
        << strerror(errno);
  }

  std::unique_ptr<CompressedSnapshotWriter> GetCowWriter() {
    const CowOptions options{.block_size = BLOCK_SIZE, .compression = "gz"};
    auto snapshot_writer = std::make_unique<CompressedSnapshotWriter>(options);
    int fd = open(cow_device_file_.path().c_str(), O_RDWR);
    EXPECT_NE(fd, -1);
    EXPECT_TRUE(snapshot_writer->SetCowDevice(unique_fd{fd}));
    snapshot_writer->SetSourceDevice(cow_source_file_.path());
    return snapshot_writer;
  }
  CowWriterFileDescriptor GetCowFd() {
    auto cow_writer = GetCowWriter();
    return CowWriterFileDescriptor{std::move(cow_writer)};
  }

  ScopedTempFile cow_source_file_{"cow_source.XXXXXX", true};
  ScopedTempFile cow_device_file_{"cow_device.XXXXXX", true};
};

TEST_F(CowWriterFileDescriptorUnittest, ReadAfterWrite) {
  std::vector<unsigned char> buffer;
  buffer.resize(BLOCK_SIZE);
  std::fill(buffer.begin(), buffer.end(), 234);

  std::vector<unsigned char> verity_data;
  verity_data.resize(BLOCK_SIZE);
  std::fill(verity_data.begin(), verity_data.end(), 0xAA);

  auto cow_writer = GetCowWriter();
  cow_writer->Initialize();

  // Simulate Writing InstallOp data
  ASSERT_TRUE(cow_writer->AddRawBlocks(0, buffer.data(), buffer.size()));
  ASSERT_TRUE(cow_writer->AddZeroBlocks(1, 2));
  ASSERT_TRUE(cow_writer->AddCopy(3, 1));
  // Fake label to simulate "end of install"
  ASSERT_TRUE(cow_writer->AddLabel(23));
  ASSERT_TRUE(
      cow_writer->AddRawBlocks(4, verity_data.data(), verity_data.size()));
  ASSERT_TRUE(cow_writer->Finalize());

  cow_writer = GetCowWriter();
  ASSERT_NE(nullptr, cow_writer);
  ASSERT_TRUE(cow_writer->InitializeAppend(23));
  auto cow_fd =
      std::make_unique<CowWriterFileDescriptor>(std::move(cow_writer));

  ASSERT_EQ((ssize_t)BLOCK_SIZE * 4, cow_fd->Seek(BLOCK_SIZE * 4, SEEK_SET));
  std::vector<unsigned char> read_back(4096);
  ASSERT_EQ((ssize_t)read_back.size(),
            cow_fd->Read(read_back.data(), read_back.size()));
  ASSERT_EQ(verity_data, read_back);

  // Since we didn't write anything to this instance of cow_fd, destructor
  // should not call Finalize(). As finalize will drop ops after resume label,
  // causing subsequent reads to fail.
  cow_writer = GetCowWriter();
  ASSERT_NE(nullptr, cow_writer);
  ASSERT_TRUE(cow_writer->InitializeAppend(23));
  cow_fd = std::make_unique<CowWriterFileDescriptor>(std::move(cow_writer));

  ASSERT_EQ((ssize_t)BLOCK_SIZE * 4, cow_fd->Seek(BLOCK_SIZE * 4, SEEK_SET));
  ASSERT_EQ((ssize_t)read_back.size(),
            cow_fd->Read(read_back.data(), read_back.size()));
  ASSERT_EQ(verity_data, read_back)
      << "Could not read verity data afeter InitializeAppend() => Read() => "
         "InitializeAppend() sequence. If no writes happened while CowWriterFd "
         "is open, Finalize() should not be called.";
}

}  // namespace chromeos_update_engine
