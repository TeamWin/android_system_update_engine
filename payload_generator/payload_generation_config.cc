// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/payload_generation_config.h"

#include "update_engine/delta_performer.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

bool ImageConfig::ValidateRootfsExists() const {
  TEST_AND_RETURN_FALSE(!rootfs_part.empty());
  TEST_AND_RETURN_FALSE(utils::FileExists(rootfs_part.c_str()));
  TEST_AND_RETURN_FALSE(rootfs_size > 0);
  // The requested size is within the limits of the file.
  TEST_AND_RETURN_FALSE(static_cast<off_t>(rootfs_size) <=
                        utils::FileSize(rootfs_part.c_str()));
  return true;
}

bool ImageConfig::ValidateKernelExists() const {
  TEST_AND_RETURN_FALSE(!kernel_part.empty());
  TEST_AND_RETURN_FALSE(utils::FileExists(kernel_part.c_str()));
  TEST_AND_RETURN_FALSE(kernel_size > 0);
  TEST_AND_RETURN_FALSE(static_cast<off_t>(kernel_size) <=
                        utils::FileSize(kernel_part.c_str()));
  return true;
}

bool ImageConfig::ValidateIsEmpty() const {
  TEST_AND_RETURN_FALSE(ImageInfoIsEmpty());

  TEST_AND_RETURN_FALSE(rootfs_part.empty());
  TEST_AND_RETURN_FALSE(rootfs_size == 0);
  TEST_AND_RETURN_FALSE(rootfs_mountpt.empty());
  TEST_AND_RETURN_FALSE(kernel_part.empty());
  TEST_AND_RETURN_FALSE(kernel_size == 0);
  return true;
}

bool ImageConfig::LoadImageSize() {
  TEST_AND_RETURN_FALSE(!rootfs_part.empty());
  int rootfs_block_count, rootfs_block_size;
  TEST_AND_RETURN_FALSE(utils::GetFilesystemSize(rootfs_part,
                                                 &rootfs_block_count,
                                                 &rootfs_block_size));
  rootfs_size = static_cast<size_t>(rootfs_block_count) * rootfs_block_size;
  if (!kernel_part.empty())
    kernel_size = utils::FileSize(kernel_part);

  // TODO(deymo): The delta generator algorithm doesn't support a block size
  // different than 4 KiB. Remove this check once that's fixed. crbug.com/455045
  if (rootfs_block_size != 4096) {
    LOG(ERROR) << "The filesystem provided in " << rootfs_part
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
    TEST_AND_RETURN_FALSE(source.ValidateRootfsExists());
    TEST_AND_RETURN_FALSE(source.rootfs_size % block_size == 0);

    if (!source.kernel_part.empty()) {
      TEST_AND_RETURN_FALSE(source.ValidateKernelExists());
      TEST_AND_RETURN_FALSE(source.kernel_size % block_size == 0);
    }

    // For deltas, we also need to check that the image is mounted in order to
    // inspect the contents.
    TEST_AND_RETURN_FALSE(!source.rootfs_mountpt.empty());
    TEST_AND_RETURN_FALSE(utils::IsDir(source.rootfs_mountpt.c_str()));

    TEST_AND_RETURN_FALSE(!target.rootfs_mountpt.empty());
    TEST_AND_RETURN_FALSE(utils::IsDir(target.rootfs_mountpt.c_str()));

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
  TEST_AND_RETURN_FALSE(target.ValidateRootfsExists());
  TEST_AND_RETURN_FALSE(target.ValidateKernelExists());
  TEST_AND_RETURN_FALSE(target.rootfs_size % block_size == 0);
  TEST_AND_RETURN_FALSE(target.kernel_size % block_size == 0);

  TEST_AND_RETURN_FALSE(chunk_size == -1 || chunk_size % block_size == 0);

  TEST_AND_RETURN_FALSE(rootfs_partition_size % block_size == 0);
  TEST_AND_RETURN_FALSE(rootfs_partition_size >= target.rootfs_size);

  return true;
}

}  // namespace chromeos_update_engine
