// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/blob_file_writer.h"

#include <string>

#include <gtest/gtest.h>

#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using chromeos_update_engine::test_utils::FillWithData;
using std::string;

namespace chromeos_update_engine {

class BlobFileWriterTest : public ::testing::Test {};

TEST(BlobFileWriterTest, SimpleTest) {
  string blob_path;
  int blob_fd;
  EXPECT_TRUE(utils::MakeTempFile("BlobFileWriterTest.XXXXXX",
                                  &blob_path,
                                  &blob_fd));
  off_t blob_file_size = 0;
  BlobFileWriter blob_file(blob_fd, &blob_file_size);

  off_t blob_size = 1024;
  chromeos::Blob blob(blob_size);
  FillWithData(&blob);
  EXPECT_EQ(0, blob_file.StoreBlob(blob));
  EXPECT_EQ(blob_size, blob_file.StoreBlob(blob));

  chromeos::Blob stored_blob(blob_size);
  ssize_t bytes_read;
  ASSERT_TRUE(utils::PReadAll(blob_fd,
                              stored_blob.data(),
                              blob_size,
                              0,
                              &bytes_read));
  EXPECT_EQ(bytes_read, blob_size);
  EXPECT_EQ(blob, stored_blob);
}

}  // namespace chromeos_update_engine
