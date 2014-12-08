// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/bzip_extent_writer.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
const char kPathTemplate[] = "./BzipExtentWriterTest-file.XXXXXX";
const uint32_t kBlockSize = 4096;
}

class BzipExtentWriterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memcpy(path_, kPathTemplate, sizeof(kPathTemplate));
    fd_.reset(new EintrSafeFileDescriptor);
    int fd = mkstemp(path_);
    ASSERT_TRUE(fd_->Open(path_, O_RDWR, 0600));
    close(fd);
  }
  void TearDown() override {
    fd_->Close();
    unlink(path_);
  }
  void WriteAlignedExtents(size_t chunk_size, size_t first_chunk_size);
  void TestZeroPad(bool aligned_size);

  FileDescriptorPtr fd_;
  char path_[sizeof(kPathTemplate)];
};

TEST_F(BzipExtentWriterTest, SimpleTest) {
  vector<Extent> extents;
  Extent extent;
  extent.set_start_block(0);
  extent.set_num_blocks(1);
  extents.push_back(extent);

  // 'echo test | bzip2 | hexdump' yields:
  static const char test_uncompressed[] = "test\n";
  static const unsigned char test[] = {
    0x42, 0x5a, 0x68, 0x39, 0x31, 0x41, 0x59, 0x26, 0x53, 0x59, 0xcc, 0xc3,
    0x71, 0xd4, 0x00, 0x00, 0x02, 0x41, 0x80, 0x00, 0x10, 0x02, 0x00, 0x0c,
    0x00, 0x20, 0x00, 0x21, 0x9a, 0x68, 0x33, 0x4d, 0x19, 0x97, 0x8b, 0xb9,
    0x22, 0x9c, 0x28, 0x48, 0x66, 0x61, 0xb8, 0xea, 0x00,
  };

  DirectExtentWriter direct_writer;
  BzipExtentWriter bzip_writer(&direct_writer);
  EXPECT_TRUE(bzip_writer.Init(fd_, extents, kBlockSize));
  EXPECT_TRUE(bzip_writer.Write(test, sizeof(test)));
  EXPECT_TRUE(bzip_writer.End());

  vector<char> buf;
  EXPECT_TRUE(utils::ReadFile(path_, &buf));
  EXPECT_EQ(strlen(test_uncompressed), buf.size());
  EXPECT_EQ(string(buf.data(), buf.size()), string(test_uncompressed));
}

TEST_F(BzipExtentWriterTest, ChunkedTest) {
  const vector<char>::size_type kDecompressedLength = 2048 * 1024;  // 2 MiB
  string decompressed_path;
  ASSERT_TRUE(utils::MakeTempFile("BzipExtentWriterTest-decompressed-XXXXXX",
                                  &decompressed_path, nullptr));
  string compressed_path;
  ASSERT_TRUE(utils::MakeTempFile("BzipExtentWriterTest-compressed-XXXXXX",
                                  &compressed_path, nullptr));
  const size_t kChunkSize = 3;

  vector<Extent> extents;
  Extent extent;
  extent.set_start_block(0);
  extent.set_num_blocks(kDecompressedLength / kBlockSize + 1);
  extents.push_back(extent);

  vector<char> decompressed_data(kDecompressedLength);
  test_utils::FillWithData(&decompressed_data);

  EXPECT_TRUE(test_utils::WriteFileVector(
      decompressed_path, decompressed_data));

  EXPECT_EQ(0, test_utils::System(
      string("cat ") + decompressed_path + "|bzip2>" + compressed_path));

  vector<char> compressed_data;
  EXPECT_TRUE(utils::ReadFile(compressed_path, &compressed_data));

  DirectExtentWriter direct_writer;
  BzipExtentWriter bzip_writer(&direct_writer);
  EXPECT_TRUE(bzip_writer.Init(fd_, extents, kBlockSize));

  vector<char> original_compressed_data = compressed_data;
  for (vector<char>::size_type i = 0; i < compressed_data.size();
       i += kChunkSize) {
    size_t this_chunk_size = min(kChunkSize, compressed_data.size() - i);
    EXPECT_TRUE(bzip_writer.Write(&compressed_data[i], this_chunk_size));
  }
  EXPECT_TRUE(bzip_writer.End());

  // Check that the const input has not been clobbered.
  test_utils::ExpectVectorsEq(original_compressed_data, compressed_data);

  vector<char> output;
  EXPECT_TRUE(utils::ReadFile(path_, &output));
  EXPECT_EQ(kDecompressedLength, output.size());
  test_utils::ExpectVectorsEq(decompressed_data, output);

  unlink(decompressed_path.c_str());
  unlink(compressed_path.c_str());
}

}  // namespace chromeos_update_engine
