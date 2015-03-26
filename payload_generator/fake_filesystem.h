// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_FAKE_FILESYSTEM_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_FAKE_FILESYSTEM_H_

// A fake filesystem interface implementation allowing the user to add arbitrary
// files/metadata.

#include "update_engine/payload_generator/filesystem_interface.h"

#include <string>
#include <vector>

#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

class FakeFilesystem : public FilesystemInterface {
 public:
  FakeFilesystem(uint64_t block_size, uint64_t block_count);
  virtual ~FakeFilesystem() = default;

  // FilesystemInterface overrides.
  size_t GetBlockSize() const override;
  size_t GetBlockCount() const override;
  bool GetFiles(std::vector<File>* files) const override;

  // Fake methods.

  // Add a file to the list of fake files.
  void AddFile(const std::string& filename, const std::vector<Extent> extents);

 private:
  FakeFilesystem() = default;

  uint64_t block_size_;
  uint64_t block_count_;

  std::vector<File> files_;

  DISALLOW_COPY_AND_ASSIGN(FakeFilesystem);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_FAKE_FILESYSTEM_H_
