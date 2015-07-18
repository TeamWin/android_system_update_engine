// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/ab_generator.h"

#include <algorithm>

#include <base/strings/stringprintf.h>

#include "update_engine/bzip.h"
#include "update_engine/delta_performer.h"
#include "update_engine/payload_generator/annotated_operation.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/delta_diff_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

bool ABGenerator::GenerateOperations(
    const PayloadGenerationConfig& config,
    int data_file_fd,
    off_t* data_file_size,
    vector<AnnotatedOperation>* rootfs_ops,
    vector<AnnotatedOperation>* kernel_ops) {

  ssize_t hard_chunk_blocks = (config.hard_chunk_size == -1 ? -1 :
                               config.hard_chunk_size / config.block_size);
  size_t soft_chunk_blocks = config.soft_chunk_size / config.block_size;

  rootfs_ops->clear();
  TEST_AND_RETURN_FALSE(diff_utils::DeltaReadPartition(
      rootfs_ops,
      config.source.rootfs,
      config.target.rootfs,
      hard_chunk_blocks,
      soft_chunk_blocks,
      data_file_fd,
      data_file_size,
      true));  // src_ops_allowed
  LOG(INFO) << "done reading normal files";

  // Read kernel partition
  TEST_AND_RETURN_FALSE(diff_utils::DeltaReadPartition(
      kernel_ops,
      config.source.kernel,
      config.target.kernel,
      hard_chunk_blocks,
      soft_chunk_blocks,
      data_file_fd,
      data_file_size,
      true));  // src_ops_allowed
  LOG(INFO) << "done reading kernel";

  TEST_AND_RETURN_FALSE(FragmentOperations(rootfs_ops,
                                           config.target.rootfs.path,
                                           data_file_fd,
                                           data_file_size));
  TEST_AND_RETURN_FALSE(FragmentOperations(kernel_ops,
                                           config.target.kernel.path,
                                           data_file_fd,
                                           data_file_size));
  SortOperationsByDestination(rootfs_ops);
  SortOperationsByDestination(kernel_ops);

  // Use the soft_chunk_size when merging operations to prevent merging all
  // the operations into a huge one if there's no hard limit.
  size_t merge_chunk_blocks = soft_chunk_blocks;
  if (hard_chunk_blocks != -1 &&
      static_cast<size_t>(hard_chunk_blocks) < soft_chunk_blocks) {
    merge_chunk_blocks = hard_chunk_blocks;
  }

  TEST_AND_RETURN_FALSE(MergeOperations(rootfs_ops,
                                        merge_chunk_blocks,
                                        config.target.rootfs.path,
                                        data_file_fd,
                                        data_file_size));
  TEST_AND_RETURN_FALSE(MergeOperations(kernel_ops,
                                        merge_chunk_blocks,
                                        config.target.kernel.path,
                                        data_file_fd,
                                        data_file_size));
  return true;
}

void ABGenerator::SortOperationsByDestination(
    vector<AnnotatedOperation>* aops) {
  sort(aops->begin(), aops->end(), diff_utils::CompareAopsByDestination);
}

bool ABGenerator::FragmentOperations(
    vector<AnnotatedOperation>* aops,
    const string& target_part_path,
    int data_fd,
    off_t* data_file_size) {
  vector<AnnotatedOperation> fragmented_aops;
  for (const AnnotatedOperation& aop : *aops) {
    if (aop.op.type() ==
        DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY) {
      TEST_AND_RETURN_FALSE(SplitSourceCopy(aop, &fragmented_aops));
    } else if ((aop.op.type() ==
                DeltaArchiveManifest_InstallOperation_Type_REPLACE) ||
               (aop.op.type() ==
                DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ)) {
      TEST_AND_RETURN_FALSE(SplitReplaceOrReplaceBz(aop, &fragmented_aops,
                                                    target_part_path, data_fd,
                                                    data_file_size));
    } else {
      fragmented_aops.push_back(aop);
    }
  }
  *aops = fragmented_aops;
  return true;
}

bool ABGenerator::SplitSourceCopy(
    const AnnotatedOperation& original_aop,
    vector<AnnotatedOperation>* result_aops) {
  DeltaArchiveManifest_InstallOperation original_op = original_aop.op;
  TEST_AND_RETURN_FALSE(original_op.type() ==
                        DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY);
  // Keeps track of the index of curr_src_ext.
  int curr_src_ext_index = 0;
  Extent curr_src_ext = original_op.src_extents(curr_src_ext_index);
  for (int i = 0; i < original_op.dst_extents_size(); i++) {
    Extent dst_ext = original_op.dst_extents(i);
    // The new operation which will have only one dst extent.
    DeltaArchiveManifest_InstallOperation new_op;
    uint64_t blocks_left = dst_ext.num_blocks();
    while (blocks_left > 0) {
      if (curr_src_ext.num_blocks() <= blocks_left) {
        // If the curr_src_ext is smaller than dst_ext, add it.
        blocks_left -= curr_src_ext.num_blocks();
        *(new_op.add_src_extents()) = curr_src_ext;
        if (curr_src_ext_index + 1 < original_op.src_extents().size()) {
          curr_src_ext = original_op.src_extents(++curr_src_ext_index);
        } else {
          break;
        }
      } else {
        // Split src_exts that are bigger than the dst_ext we're dealing with.
        Extent first_ext;
        first_ext.set_num_blocks(blocks_left);
        first_ext.set_start_block(curr_src_ext.start_block());
        *(new_op.add_src_extents()) = first_ext;
        // Keep the second half of the split op.
        curr_src_ext.set_num_blocks(curr_src_ext.num_blocks() - blocks_left);
        curr_src_ext.set_start_block(curr_src_ext.start_block() + blocks_left);
        blocks_left -= first_ext.num_blocks();
      }
    }
    // Fix up our new operation and add it to the results.
    new_op.set_type(DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY);
    *(new_op.add_dst_extents()) = dst_ext;
    new_op.set_src_length(dst_ext.num_blocks() * kBlockSize);
    new_op.set_dst_length(dst_ext.num_blocks() * kBlockSize);

    AnnotatedOperation new_aop;
    new_aop.op = new_op;
    new_aop.name = base::StringPrintf("%s:%d", original_aop.name.c_str(), i);
    result_aops->push_back(new_aop);
  }
  if (curr_src_ext_index != original_op.src_extents().size() - 1) {
    LOG(FATAL) << "Incorrectly split SOURCE_COPY operation. Did not use all "
               << "source extents.";
  }
  return true;
}

bool ABGenerator::SplitReplaceOrReplaceBz(
    const AnnotatedOperation& original_aop,
    vector<AnnotatedOperation>* result_aops,
    const string& target_part_path,
    int data_fd,
    off_t* data_file_size) {
  DeltaArchiveManifest_InstallOperation original_op = original_aop.op;
  const bool is_replace =
      original_op.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE;
  TEST_AND_RETURN_FALSE(
      is_replace ||
      (original_op.type() ==
       DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ));

  uint32_t data_offset = original_op.data_offset();
  for (int i = 0; i < original_op.dst_extents_size(); i++) {
    Extent dst_ext = original_op.dst_extents(i);
    // Make a new operation with only one dst extent.
    DeltaArchiveManifest_InstallOperation new_op;
    *(new_op.add_dst_extents()) = dst_ext;
    uint32_t data_size = dst_ext.num_blocks() * kBlockSize;
    new_op.set_dst_length(data_size);
    // If this is a REPLACE, attempt to reuse portions of the existing blob.
    if (is_replace) {
      new_op.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE);
      new_op.set_data_length(data_size);
      new_op.set_data_offset(data_offset);
      data_offset += data_size;
    }

    AnnotatedOperation new_aop;
    new_aop.op = new_op;
    new_aop.name = base::StringPrintf("%s:%d", original_aop.name.c_str(), i);
    TEST_AND_RETURN_FALSE(AddDataAndSetType(&new_aop, target_part_path, data_fd,
                                            data_file_size));

    result_aops->push_back(new_aop);
  }
  return true;
}

bool ABGenerator::MergeOperations(vector<AnnotatedOperation>* aops,
                                  size_t chunk_blocks,
                                  const string& target_part_path,
                                  int data_fd,
                                  off_t* data_file_size) {
  vector<AnnotatedOperation> new_aops;
  for (const AnnotatedOperation& curr_aop : *aops) {
    if (new_aops.empty()) {
      new_aops.push_back(curr_aop);
      continue;
    }
    AnnotatedOperation& last_aop = new_aops.back();

    if (last_aop.op.dst_extents_size() <= 0 ||
        curr_aop.op.dst_extents_size() <= 0) {
      new_aops.push_back(curr_aop);
      continue;
    }
    uint32_t last_dst_idx = last_aop.op.dst_extents_size() - 1;
    uint32_t last_end_block =
        last_aop.op.dst_extents(last_dst_idx).start_block() +
        last_aop.op.dst_extents(last_dst_idx).num_blocks();
    uint32_t curr_start_block = curr_aop.op.dst_extents(0).start_block();
    uint32_t combined_block_count =
        last_aop.op.dst_extents(last_dst_idx).num_blocks() +
        curr_aop.op.dst_extents(0).num_blocks();
    bool good_op_type =
        curr_aop.op.type() ==
            DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY ||
        curr_aop.op.type() ==
            DeltaArchiveManifest_InstallOperation_Type_REPLACE ||
        curr_aop.op.type() ==
            DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ;
    if (good_op_type &&
        last_aop.op.type() == curr_aop.op.type() &&
        last_end_block == curr_start_block &&
        combined_block_count <= chunk_blocks) {
      // If the operations have the same type (which is a type that we can
      // merge), are contiguous, are fragmented to have one destination extent,
      // and their combined block count would be less than chunk size, merge
      // them.
      last_aop.name = base::StringPrintf("%s,%s",
                                         last_aop.name.c_str(),
                                         curr_aop.name.c_str());

      ExtendExtents(last_aop.op.mutable_src_extents(),
                    curr_aop.op.src_extents());
      if (curr_aop.op.src_length() > 0)
        last_aop.op.set_src_length(last_aop.op.src_length() +
                                   curr_aop.op.src_length());
      ExtendExtents(last_aop.op.mutable_dst_extents(),
                    curr_aop.op.dst_extents());
      if (curr_aop.op.dst_length() > 0)
        last_aop.op.set_dst_length(last_aop.op.dst_length() +
                                   curr_aop.op.dst_length());
      // Set the data length to zero so we know to add the blob later.
      if (curr_aop.op.type() ==
          DeltaArchiveManifest_InstallOperation_Type_REPLACE ||
          curr_aop.op.type() ==
          DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ) {
        last_aop.op.set_data_length(0);
      }
    } else {
      // Otherwise just include the extent as is.
      new_aops.push_back(curr_aop);
    }
  }

  // Set the blobs for REPLACE/REPLACE_BZ operations that have been merged.
  for (AnnotatedOperation& curr_aop : new_aops) {
    if (curr_aop.op.data_length() == 0 &&
        (curr_aop.op.type() ==
            DeltaArchiveManifest_InstallOperation_Type_REPLACE ||
         curr_aop.op.type() ==
            DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ)) {
      TEST_AND_RETURN_FALSE(AddDataAndSetType(&curr_aop, target_part_path,
                                              data_fd, data_file_size));
    }
  }

  *aops = new_aops;
  return true;
}

bool ABGenerator::AddDataAndSetType(AnnotatedOperation* aop,
                                    const string& target_part_path,
                                    int data_fd,
                                    off_t* data_file_size) {
  TEST_AND_RETURN_FALSE(
      aop->op.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE ||
      aop->op.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);

  chromeos::Blob data(aop->op.dst_length());
  vector<Extent> dst_extents;
  ExtentsToVector(aop->op.dst_extents(), &dst_extents);
  TEST_AND_RETURN_FALSE(utils::ReadExtents(target_part_path,
                                           dst_extents,
                                           &data,
                                           data.size(),
                                           kBlockSize));

  chromeos::Blob data_bz;
  TEST_AND_RETURN_FALSE(BzipCompress(data, &data_bz));
  CHECK(!data_bz.empty());

  chromeos::Blob* data_p = nullptr;
  DeltaArchiveManifest_InstallOperation_Type new_op_type;
  if (data_bz.size() < data.size()) {
    new_op_type = DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ;
    data_p = &data_bz;
  } else {
    new_op_type = DeltaArchiveManifest_InstallOperation_Type_REPLACE;
    data_p = &data;
  }

  // If the operation already points to a data blob, check whether it's
  // identical to the new one, in which case don't add it.
  if (aop->op.type() == new_op_type &&
      aop->op.data_length() == data_p->size()) {
    chromeos::Blob current_data(data_p->size());
    ssize_t bytes_read;
    TEST_AND_RETURN_FALSE(utils::PReadAll(data_fd,
                                          current_data.data(),
                                          aop->op.data_length(),
                                          aop->op.data_offset(),
                                          &bytes_read));
    TEST_AND_RETURN_FALSE(bytes_read ==
                          static_cast<ssize_t>(aop->op.data_length()));
    if (current_data == *data_p)
      data_p = nullptr;
  }

  if (data_p) {
    aop->op.set_type(new_op_type);
    aop->SetOperationBlob(data_p, data_fd, data_file_size);
  }

  return true;
}

}  // namespace chromeos_update_engine
