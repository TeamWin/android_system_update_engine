//
// Copyright (C) 2015 The Android Open Source Project
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

#include "update_engine/payload_generator/payload_generation_config.h"

#include <base/logging.h>

#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/delta_performer.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/ext2_filesystem.h"
#include "update_engine/payload_generator/raw_filesystem.h"

namespace chromeos_update_engine {

bool PostInstallConfig::IsEmpty() const {
  return run == false && path.empty() && filesystem_type.empty();
}

bool PartitionConfig::ValidateExists() const {
  TEST_AND_RETURN_FALSE(!path.empty());
  TEST_AND_RETURN_FALSE(utils::FileExists(path.c_str()));
  TEST_AND_RETURN_FALSE(size > 0);
  // The requested size is within the limits of the file.
  TEST_AND_RETURN_FALSE(static_cast<off_t>(size) <=
                        utils::FileSize(path.c_str()));
  // TODO(deymo): The delta generator algorithm doesn't support a block size
  // different than 4 KiB. Remove this check once that's fixed. crbug.com/455045
  int block_count, block_size;
  if (utils::GetFilesystemSize(path, &block_count, &block_size) &&
      block_size != 4096) {
   LOG(ERROR) << "The filesystem provided in " << path
              << " has a block size of " << block_size
              << " but delta_generator only supports 4096.";
   return false;
  }
  return true;
}

bool PartitionConfig::OpenFilesystem() {
  if (path.empty())
    return true;
  fs_interface.reset();
  if (utils::IsExtFilesystem(path)) {
    fs_interface = Ext2Filesystem::CreateFromFile(path);
  }

  if (!fs_interface) {
    // Fall back to a RAW filesystem.
    TEST_AND_RETURN_FALSE(size % kBlockSize == 0);
    fs_interface = RawFilesystem::Create(
      "<" + name + "-partition>",
      kBlockSize,
      size / kBlockSize);
  }
  return true;
}

bool ImageConfig::ValidateIsEmpty() const {
  TEST_AND_RETURN_FALSE(ImageInfoIsEmpty());
  return partitions.empty();
}

bool ImageConfig::LoadImageSize() {
  for (PartitionConfig& part : partitions) {
    if (part.path.empty())
      continue;
    part.size = utils::FileSize(part.path);
  }
  return true;
}

bool ImageConfig::LoadPostInstallConfig(const brillo::KeyValueStore& store) {
  bool found_postinstall = false;
  for (PartitionConfig& part : partitions) {
    bool run_postinstall;
    if (!store.GetBoolean("RUN_POSTINSTALL_" + part.name, &run_postinstall) ||
        !run_postinstall)
      continue;
    found_postinstall = true;
    part.postinstall.run = true;
    store.GetString("POSTINSTALL_PATH_" + part.name, &part.postinstall.path);
    store.GetString("FILESYSTEM_TYPE_" + part.name,
                    &part.postinstall.filesystem_type);
  }
  if (!found_postinstall) {
    LOG(ERROR) << "No valid postinstall config found.";
    return false;
  }
  return true;
}

bool ImageConfig::ImageInfoIsEmpty() const {
  return image_info.board().empty()
    && image_info.key().empty()
    && image_info.channel().empty()
    && image_info.version().empty()
    && image_info.build_channel().empty()
    && image_info.build_version().empty();
}

bool PayloadGenerationConfig::Validate() const {
  if (is_delta) {
    for (const PartitionConfig& part : source.partitions) {
      if (!part.path.empty()) {
        TEST_AND_RETURN_FALSE(part.ValidateExists());
        TEST_AND_RETURN_FALSE(part.size % block_size == 0);
      }
      // Source partition should not have postinstall.
      TEST_AND_RETURN_FALSE(part.postinstall.IsEmpty());
    }

    // Check for the supported minor_version values.
    TEST_AND_RETURN_FALSE(minor_version == kInPlaceMinorPayloadVersion ||
                          minor_version == kSourceMinorPayloadVersion ||
                          minor_version == kOpSrcHashMinorPayloadVersion ||
                          minor_version == kImgdiffMinorPayloadVersion);

    if (imgdiff_allowed)
      TEST_AND_RETURN_FALSE(minor_version >= kImgdiffMinorPayloadVersion);

    // If new_image_info is present, old_image_info must be present.
    TEST_AND_RETURN_FALSE(source.ImageInfoIsEmpty() ==
                          target.ImageInfoIsEmpty());
  } else {
    // All the "source" image fields must be empty for full payloads.
    TEST_AND_RETURN_FALSE(source.ValidateIsEmpty());
    TEST_AND_RETURN_FALSE(minor_version == kFullPayloadMinorVersion);
  }

  // In all cases, the target image must exists.
  for (const PartitionConfig& part : target.partitions) {
    TEST_AND_RETURN_FALSE(part.ValidateExists());
    TEST_AND_RETURN_FALSE(part.size % block_size == 0);
    if (minor_version == kInPlaceMinorPayloadVersion &&
        part.name == kLegacyPartitionNameRoot)
      TEST_AND_RETURN_FALSE(rootfs_partition_size >= part.size);
    if (major_version == kChromeOSMajorPayloadVersion)
      TEST_AND_RETURN_FALSE(part.postinstall.IsEmpty());
  }

  TEST_AND_RETURN_FALSE(hard_chunk_size == -1 ||
                        hard_chunk_size % block_size == 0);
  TEST_AND_RETURN_FALSE(soft_chunk_size % block_size == 0);

  TEST_AND_RETURN_FALSE(rootfs_partition_size % block_size == 0);

  return true;
}

}  // namespace chromeos_update_engine
