// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_RAW_FILESYSTEM_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_RAW_FILESYSTEM_H_

// A simple filesystem interface implementation used for unknown filesystem
// format such as the kernel.

#include "update_engine/payload_generator/filesystem_interface.h"

#include <string>
#include <vector>

namespace chromeos_update_engine {

class RawFilesystem : public FilesystemInterface {
 public:
  static std::unique_ptr<RawFilesystem> Create(
      const std::string& filename, uint64_t block_size, uint64_t block_count);
  virtual ~RawFilesystem() = default;

  // FilesystemInterface overrides.
  size_t GetBlockSize() const override;
  size_t GetBlockCount() const override;

  // GetFiles will return only one file with all the blocks of the filesystem
  // with the name passed during construction.
  bool GetFiles(std::vector<File>* files) const override;

 private:
  RawFilesystem() = default;

  std::string filename_;
  uint64_t block_count_;
  uint64_t block_size_;

  DISALLOW_COPY_AND_ASSIGN(RawFilesystem);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_RAW_FILESYSTEM_H_
