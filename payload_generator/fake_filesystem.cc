// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/fake_filesystem.h"

#include <gtest/gtest.h>

namespace chromeos_update_engine {

FakeFilesystem::FakeFilesystem(uint64_t block_size, uint64_t block_count) :
    block_size_(block_size),
    block_count_(block_count) {
}

size_t FakeFilesystem::GetBlockSize() const {
  return block_size_;
}

size_t FakeFilesystem::GetBlockCount() const {
  return block_count_;
}

bool FakeFilesystem::GetFiles(std::vector<File>* files) const {
  *files = files_;
  return true;
}

void FakeFilesystem::AddFile(const std::string& filename,
                             const std::vector<Extent> extents) {
  File file;
  file.name = filename;
  file.extents = extents;
  for (const Extent& extent : extents) {
    EXPECT_LE(0, extent.start_block());
    EXPECT_LE(extent.start_block() + extent.num_blocks(), block_count_);
  }
  files_.push_back(file);
}

bool FakeFilesystem::LoadSettings(chromeos::KeyValueStore* store) const {
  if (minor_version_ < 0)
    return false;
  store->SetString("PAYLOAD_MINOR_VERSION", std::to_string(minor_version_));
  return true;
}

}  // namespace chromeos_update_engine
