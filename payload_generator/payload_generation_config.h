// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_PAYLOAD_GENERATION_CONFIG_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_PAYLOAD_GENERATION_CONFIG_H_

#include <cstddef>

#include <memory>
#include <string>
#include <vector>

#include "update_engine/payload_generator/filesystem_interface.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

// The list different kind of partitions supported by the updater.
enum class PartitionName {
  kKernel,
  kRootfs,
};

struct PartitionConfig {
  explicit PartitionConfig(PartitionName name) : name(name) {}

  // Returns whether the PartitionConfig is not an empty image and all the
  // fields are set correctly to a valid image file.
  bool ValidateExists() const;

  // Open then filesystem stored in this partition and stores it in
  // |fs_interface|. Returns whether opening the filesystem worked.
  bool OpenFilesystem();

  // The path to the partition file. This can be a regular file or a block
  // device such as a loop device.
  std::string path;

  // The size of the data in |path|. If rootfs verification is used (verity)
  // this value should match the size of the verity device for the rootfs, and
  // the size of the whole kernel. This value could be smaller than the
  // partition and is the size of the data update_engine assumes verified for
  // the source image, and the size of that data it should generate for the
  // target image.
  uint64_t size = 0;

  // The FilesystemInterface implementation used to access this partition's
  // files.
  std::unique_ptr<FilesystemInterface> fs_interface;

  PartitionName name;
};

// The ImageConfig struct describes a pair of binaries kernel and rootfs and the
// metadata associated with the image they are part of, like build number, size,
// etc.
struct ImageConfig {
  // Returns whether the ImageConfig is an empty image.
  bool ValidateIsEmpty() const;

  // Load |rootfs_size| and |kernel.size| from the respective image files. For
  // the kernel, the whole |kernel.path| file is assumed. For the rootfs, the
  // size is detected from the filesystem.
  // Returns whether the image size was properly detected.
  bool LoadImageSize();

  // Load the |rootfs_size| stored in the kernel command line in the
  // |kernel.path| when the kernel is using rootfs verification (dm-verity).
  // Returns whether it loaded the size from the kernel command line. For
  // example, it would return false if no |kernel.path| was provided or the
  // kernel doesn't have verity enabled.
  bool LoadVerityRootfsSize();

  // Returns whether the |image_info| field is empty.
  bool ImageInfoIsEmpty() const;

  // The ImageInfo message defined in the update_metadata.proto file describes
  // the metadata of the image.
  ImageInfo image_info;

  // The updated partitions.
  PartitionConfig rootfs = PartitionConfig{PartitionName::kRootfs};
  PartitionConfig kernel = PartitionConfig{PartitionName::kKernel};
};

// The PayloadGenerationConfig struct encapsulates all the configuration to
// build the requested payload. This includes information about the old and new
// image as well as the restrictions applied to the payload (like minor-version
// and full/delta payload).
struct PayloadGenerationConfig {
  // Returns whether the PayloadGenerationConfig is valid.
  bool Validate() const;

  // Image information about the new image that's the target of this payload.
  ImageConfig target;

  // Image information pertaining the old image, if any. This is only valid
  // if is_full is false, so we are requested a delta payload.
  ImageConfig source;

  // Wheter the requested payload is a delta payload.
  bool is_delta = false;

  // The minor_version of the requested payload.
  uint32_t minor_version;

  // The size of the rootfs partition, that not necessarily is the same as the
  // filesystem in either source or target version, since there is some space
  // after the partition used to store the verity hashes and or the bootcache.
  uint64_t rootfs_partition_size = 0;

  // The chunk size is the maximum size that a single operation should write in
  // the destination. Operations bigger than chunk_size should be split. A value
  // of -1 means no chunk_size limit.
  off_t chunk_size = -1;

  // TODO(deymo): Remove the block_size member and maybe replace it with a
  // minimum alignment size for blocks (if needed). Algorithms should be able to
  // pick the block_size they want, but for now only 4 KiB is supported.

  // The block size used for all the operations in the manifest.
  size_t block_size = 4096;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_PAYLOAD_GENERATION_CONFIG_H_
