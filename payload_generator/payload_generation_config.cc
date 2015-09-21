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

#include "update_engine/delta_performer.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/ext2_filesystem.h"
#include "update_engine/payload_generator/raw_filesystem.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

bool PartitionConfig::ValidateExists() const {
  TEST_AND_RETURN_FALSE(!path.empty());
  TEST_AND_RETURN_FALSE(utils::FileExists(path.c_str()));
  TEST_AND_RETURN_FALSE(size > 0);
  // The requested size is within the limits of the file.
  TEST_AND_RETURN_FALSE(static_cast<off_t>(size) <=
                        utils::FileSize(path.c_str()));
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

  TEST_AND_RETURN_FALSE(rootfs.path.empty());
  TEST_AND_RETURN_FALSE(rootfs.size == 0);
  TEST_AND_RETURN_FALSE(kernel.path.empty());
  TEST_AND_RETURN_FALSE(kernel.size == 0);
  return true;
}

bool ImageConfig::LoadImageSize() {
  TEST_AND_RETURN_FALSE(!rootfs.path.empty());
  int rootfs_block_count, rootfs_block_size;
  TEST_AND_RETURN_FALSE(utils::GetFilesystemSize(rootfs.path,
                                                 &rootfs_block_count,
                                                 &rootfs_block_size));
  rootfs.size = static_cast<uint64_t>(rootfs_block_count) * rootfs_block_size;
  if (!kernel.path.empty())
    kernel.size = utils::FileSize(kernel.path);

  // TODO(deymo): The delta generator algorithm doesn't support a block size
  // different than 4 KiB. Remove this check once that's fixed. crbug.com/455045
  if (rootfs_block_size != 4096) {
    LOG(ERROR) << "The filesystem provided in " << rootfs.path
               << " has a block size of " << rootfs_block_size
               << " but delta_generator only supports 4096.";
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
    TEST_AND_RETURN_FALSE(source.rootfs.ValidateExists());
    TEST_AND_RETURN_FALSE(source.rootfs.size % block_size == 0);

    if (!source.kernel.path.empty()) {
      TEST_AND_RETURN_FALSE(source.kernel.ValidateExists());
      TEST_AND_RETURN_FALSE(source.kernel.size % block_size == 0);
    }

    // Check for the supported minor_version values.
    TEST_AND_RETURN_FALSE(minor_version == kInPlaceMinorPayloadVersion ||
                          minor_version == kSourceMinorPayloadVersion);

    // If new_image_info is present, old_image_info must be present.
    TEST_AND_RETURN_FALSE(source.ImageInfoIsEmpty() ==
                          target.ImageInfoIsEmpty());
  } else {
    // All the "source" image fields must be empty for full payloads.
    TEST_AND_RETURN_FALSE(source.ValidateIsEmpty());
    TEST_AND_RETURN_FALSE(minor_version ==
                          DeltaPerformer::kFullPayloadMinorVersion);
  }

  // In all cases, the target image must exists.
  TEST_AND_RETURN_FALSE(target.rootfs.ValidateExists());
  TEST_AND_RETURN_FALSE(target.kernel.ValidateExists());
  TEST_AND_RETURN_FALSE(target.rootfs.size % block_size == 0);
  TEST_AND_RETURN_FALSE(target.kernel.size % block_size == 0);

  TEST_AND_RETURN_FALSE(hard_chunk_size == -1 ||
                        hard_chunk_size % block_size == 0);
  TEST_AND_RETURN_FALSE(soft_chunk_size % block_size == 0);

  TEST_AND_RETURN_FALSE(rootfs_partition_size % block_size == 0);
  TEST_AND_RETURN_FALSE(rootfs_partition_size >= target.rootfs.size);

  return true;
}

}  // namespace chromeos_update_engine
