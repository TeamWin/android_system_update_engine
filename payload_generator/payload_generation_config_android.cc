//
// Copyright (C) 2018 The Android Open Source Project
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
#include <brillo/secure_blob.h>
#include <libavb/libavb.h>
#include <verity/hash_tree_builder.h>

#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/verity_writer_android.h"
#include "update_engine/payload_generator/extent_ranges.h"

namespace chromeos_update_engine {

namespace {
bool AvbDescriptorCallback(const AvbDescriptor* descriptor, void* user_data) {
  PartitionConfig* part = static_cast<PartitionConfig*>(user_data);
  AvbDescriptor desc;
  TEST_AND_RETURN_FALSE(
      avb_descriptor_validate_and_byteswap(descriptor, &desc));
  if (desc.tag != AVB_DESCRIPTOR_TAG_HASHTREE)
    return true;

  AvbHashtreeDescriptor hashtree;
  TEST_AND_RETURN_FALSE(avb_hashtree_descriptor_validate_and_byteswap(
      reinterpret_cast<const AvbHashtreeDescriptor*>(descriptor), &hashtree));
  // We only support version 1 right now, will need to introduce a new
  // payload minor version to support new dm verity version.
  TEST_AND_RETURN_FALSE(hashtree.dm_verity_version == 1);
  part->verity.hash_tree_algorithm =
      reinterpret_cast<const char*>(hashtree.hash_algorithm);

  const uint8_t* salt = reinterpret_cast<const uint8_t*>(descriptor) +
                        sizeof(AvbHashtreeDescriptor) +
                        hashtree.partition_name_len;
  part->verity.hash_tree_salt.assign(salt, salt + hashtree.salt_len);

  TEST_AND_RETURN_FALSE(hashtree.data_block_size ==
                        part->fs_interface->GetBlockSize());
  part->verity.hash_tree_data_extent =
      ExtentForBytes(hashtree.data_block_size, 0, hashtree.image_size);

  TEST_AND_RETURN_FALSE(hashtree.hash_block_size ==
                        part->fs_interface->GetBlockSize());

  // Generate hash tree based on the descriptor and verify that it matches
  // the hash tree stored in the image.
  auto hash_function =
      HashTreeBuilder::HashFunction(part->verity.hash_tree_algorithm);
  TEST_AND_RETURN_FALSE(hash_function != nullptr);
  HashTreeBuilder hash_tree_builder(hashtree.data_block_size, hash_function);
  TEST_AND_RETURN_FALSE(hash_tree_builder.Initialize(
      hashtree.image_size, part->verity.hash_tree_salt));
  TEST_AND_RETURN_FALSE(hash_tree_builder.CalculateSize(hashtree.image_size) ==
                        hashtree.tree_size);

  brillo::Blob buffer;
  for (uint64_t offset = 0; offset < hashtree.image_size;) {
    constexpr uint64_t kBufferSize = 1024 * 1024;
    size_t bytes_to_read = std::min(kBufferSize, hashtree.image_size - offset);
    TEST_AND_RETURN_FALSE(
        utils::ReadFileChunk(part->path, offset, bytes_to_read, &buffer));
    TEST_AND_RETURN_FALSE(
        hash_tree_builder.Update(buffer.data(), buffer.size()));
    offset += buffer.size();
    buffer.clear();
  }
  TEST_AND_RETURN_FALSE(hash_tree_builder.BuildHashTree());
  TEST_AND_RETURN_FALSE(utils::ReadFileChunk(
      part->path, hashtree.tree_offset, hashtree.tree_size, &buffer));
  TEST_AND_RETURN_FALSE(hash_tree_builder.CheckHashTree(buffer));

  part->verity.hash_tree_extent = ExtentForBytes(
      hashtree.hash_block_size, hashtree.tree_offset, hashtree.tree_size);

  TEST_AND_RETURN_FALSE(VerityWriterAndroid::EncodeFEC(part->path,
                                                       0 /* data_offset */,
                                                       hashtree.fec_offset,
                                                       hashtree.fec_offset,
                                                       hashtree.fec_size,
                                                       hashtree.fec_num_roots,
                                                       hashtree.data_block_size,
                                                       true /* verify_mode */));

  part->verity.fec_data_extent =
      ExtentForBytes(hashtree.data_block_size, 0, hashtree.fec_offset);
  part->verity.fec_extent = ExtentForBytes(
      hashtree.data_block_size, hashtree.fec_offset, hashtree.fec_size);
  part->verity.fec_roots = hashtree.fec_num_roots;
  return true;
}
}  // namespace

bool ImageConfig::LoadVerityConfig() {
  for (PartitionConfig& part : partitions) {
    if (part.size < sizeof(AvbFooter))
      continue;
    uint64_t footer_offset = part.size - sizeof(AvbFooter);
    brillo::Blob buffer;
    TEST_AND_RETURN_FALSE(utils::ReadFileChunk(
        part.path, footer_offset, sizeof(AvbFooter), &buffer));
    if (memcmp(buffer.data(), AVB_FOOTER_MAGIC, AVB_FOOTER_MAGIC_LEN) != 0)
      continue;
    LOG(INFO) << "Parsing verity config from AVB footer for " << part.name;
    AvbFooter footer;
    TEST_AND_RETURN_FALSE(avb_footer_validate_and_byteswap(
        reinterpret_cast<const AvbFooter*>(buffer.data()), &footer));
    buffer.clear();

    TEST_AND_RETURN_FALSE(footer.vbmeta_offset + sizeof(AvbVBMetaImageHeader) <=
                          part.size);
    TEST_AND_RETURN_FALSE(utils::ReadFileChunk(
        part.path, footer.vbmeta_offset, footer.vbmeta_size, &buffer));
    TEST_AND_RETURN_FALSE(avb_descriptor_foreach(
        buffer.data(), buffer.size(), AvbDescriptorCallback, &part));
  }
  return true;
}

}  // namespace chromeos_update_engine
