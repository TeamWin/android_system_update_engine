//
// Copyright (C) 2017 The Android Open Source Project
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

#include "update_engine/payload_generator/deflate_utils.h"

#include <algorithm>
#include <string>
#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>

#include "update_engine/common/utils.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/extent_ranges.h"
#include "update_engine/payload_generator/extent_utils.h"
#include "update_engine/payload_generator/squashfs_filesystem.h"
#include "update_engine/update_metadata.pb.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {
namespace deflate_utils {
namespace {

// The minimum size for a squashfs image to be processed.
const uint64_t kMinimumSquashfsImageSize = 1 * 1024 * 1024;  // bytes

// TODO(*): Optimize this so we don't have to read all extents into memory in
// case it is large.
bool CopyExtentsToFile(const string& in_path,
                       const vector<Extent> extents,
                       const string& out_path,
                       size_t block_size) {
  brillo::Blob data(BlocksInExtents(extents) * block_size);
  TEST_AND_RETURN_FALSE(
      utils::ReadExtents(in_path, extents, &data, data.size(), block_size));
  TEST_AND_RETURN_FALSE(
      utils::WriteFile(out_path.c_str(), data.data(), data.size()));
  return true;
}

bool IsSquashfsImage(const string& part_path,
                     const FilesystemInterface::File& file) {
  // Only check for files with img postfix.
  if (base::EndsWith(file.name, ".img", base::CompareCase::SENSITIVE) &&
      BlocksInExtents(file.extents) >= kMinimumSquashfsImageSize / kBlockSize) {
    brillo::Blob super_block;
    TEST_AND_RETURN_FALSE(
        utils::ReadFileChunk(part_path,
                             file.extents[0].start_block() * kBlockSize,
                             100,
                             &super_block));
    return SquashfsFilesystem::IsSquashfsImage(super_block);
  }
  return false;
}

// Realigns subfiles |files| of a splitted file |file| into its correct
// positions. This can be used for squashfs, zip, apk, etc.
bool RealignSplittedFiles(const FilesystemInterface::File& file,
                          vector<FilesystemInterface::File>* files) {
  // We have to shift all the Extents in |files|, based on the Extents of the
  // |file| itself.
  size_t num_blocks = 0;
  for (auto& in_file : *files) {  // We need to modify so no constant.
    TEST_AND_RETURN_FALSE(
        ShiftExtentsOverExtents(file.extents, &in_file.extents));
    in_file.name = file.name + "/" + in_file.name;
    num_blocks += BlocksInExtents(in_file.extents);
  }

  // Check that all files in |in_files| cover the entire image.
  TEST_AND_RETURN_FALSE(BlocksInExtents(file.extents) == num_blocks);
  return true;
}

}  // namespace

bool ShiftExtentsOverExtents(const vector<Extent>& base_extents,
                             vector<Extent>* over_extents) {
  if (BlocksInExtents(base_extents) < BlocksInExtents(*over_extents)) {
    LOG(ERROR) << "over_extents have more blocks than base_extents! Invalid!";
    return false;
  }
  for (size_t idx = 0; idx < over_extents->size(); idx++) {
    auto over_ext = &over_extents->at(idx);
    auto gap_blocks = base_extents[0].start_block();
    auto last_end_block = base_extents[0].start_block();
    for (auto base_ext : base_extents) {  // We need to modify |base_ext|, so we
                                          // use copy.
      gap_blocks += base_ext.start_block() - last_end_block;
      last_end_block = base_ext.start_block() + base_ext.num_blocks();
      base_ext.set_start_block(base_ext.start_block() - gap_blocks);
      if (over_ext->start_block() >= base_ext.start_block() &&
          over_ext->start_block() <
              base_ext.start_block() + base_ext.num_blocks()) {
        if (over_ext->start_block() + over_ext->num_blocks() <=
            base_ext.start_block() + base_ext.num_blocks()) {
          // |over_ext| is inside |base_ext|, increase its start block.
          over_ext->set_start_block(over_ext->start_block() + gap_blocks);
        } else {
          // |over_ext| spills over this |base_ext|, split it into two.
          auto new_blocks = base_ext.start_block() + base_ext.num_blocks() -
                            over_ext->start_block();
          vector<Extent> new_extents = {
              ExtentForRange(gap_blocks + over_ext->start_block(), new_blocks),
              ExtentForRange(over_ext->start_block() + new_blocks,
                             over_ext->num_blocks() - new_blocks)};
          *over_ext = new_extents[0];
          over_extents->insert(std::next(over_extents->begin(), idx + 1),
                               new_extents[1]);
        }
        break;  // We processed |over_ext|, so break the loop;
      }
    }
  }
  return true;
}

bool PreprocessParitionFiles(const PartitionConfig& part,
                             vector<FilesystemInterface::File>* result_files) {
  // Get the file system files.
  vector<FilesystemInterface::File> tmp_files;
  part.fs_interface->GetFiles(&tmp_files);
  result_files->reserve(tmp_files.size());

  for (const auto& file : tmp_files) {
    if (IsSquashfsImage(part.path, file)) {
      // Read the image into a file.
      base::FilePath path;
      TEST_AND_RETURN_FALSE(base::CreateTemporaryFile(&path));
      ScopedPathUnlinker old_unlinker(path.value());
      TEST_AND_RETURN_FALSE(
          CopyExtentsToFile(part.path, file.extents, path.value(), kBlockSize));
      // Test if it is actually a Squashfs file.
      auto sqfs = SquashfsFilesystem::CreateFromFile(path.value());
      if (sqfs) {
        // It is an squashfs file. Get its files to replace with itself.
        vector<FilesystemInterface::File> files;
        sqfs->GetFiles(&files);

        // Replace squashfs file with its files only if |files| has at
        // least two files.
        if (files.size() > 1) {
          TEST_AND_RETURN_FALSE(RealignSplittedFiles(file, &files));
          result_files->insert(result_files->end(), files.begin(), files.end());
          continue;
        }
      } else {
        LOG(WARNING) << "We thought file: " << file.name
                     << " was a Squashfs file, but it was not.";
      }
    }
    // TODO(ahassani): Process other types of files like apk, zip, etc.
    result_files->push_back(file);
  }

  return true;
}

}  // namespace deflate_utils
}  // namespace chromeos_update_engine
