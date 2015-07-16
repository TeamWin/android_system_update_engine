// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_UTILS_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_UTILS_H_

#include <string>
#include <vector>

#include <chromeos/secure_blob.h>

#include "update_engine/payload_generator/annotated_operation.h"
#include "update_engine/payload_generator/extent_ranges.h"
#include "update_engine/payload_generator/payload_generation_config.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

namespace diff_utils {

// Create operations in |aops| to produce all the blocks in the |new_part|
// partition using the filesystem opened in that PartitionConfig.
// It uses the files reported by the filesystem in |old_part| and the data
// blocks in that partition (if available) to determine the best way to compress
// the new files (REPLACE, REPLACE_BZ, COPY, BSDIFF) and writes any necessary
// data to the end of |data_fd| updating |data_file_size| accordingly.
bool DeltaReadPartition(std::vector<AnnotatedOperation>* aops,
                        const PartitionConfig& old_part,
                        const PartitionConfig& new_part,
                        off_t chunk_blocks,
                        int data_fd,
                        off_t* data_file_size,
                        bool skip_block_0,
                        bool src_ops_allowed);

// Create operations in |aops| for identical blocks that moved around in the old
// and new partition and also handle zeroed blocks. The old and new partition
// are stored in the |old_part| and |new_part| files and have |old_num_blocks|
// and |new_num_blocks| respectively. The maximum operation size is
// |chunk_blocks| blocks, or unlimited if |chunk_blocks| is -1. The blobs of the
// produced operations are stored in the |data_fd| file whose size is updated
// in the value pointed by |data_file_size|.
// The collections |old_visited_blocks| and |new_visited_blocks| state what
// blocks already have operations reading or writing them and only operations
// for unvisited blocks are produced by this function updating both collections
// with the used blocks.
bool DeltaMovedAndZeroBlocks(std::vector<AnnotatedOperation>* aops,
                             const std::string& old_part,
                             const std::string& new_part,
                             size_t old_num_blocks,
                             size_t new_num_blocks,
                             off_t chunk_blocks,
                             bool src_ops_allowed,
                             int data_fd,
                             off_t* data_file_size,
                             ExtentRanges* old_visited_blocks,
                             ExtentRanges* new_visited_blocks);

// For a given file |name| append operations to |aops| to produce it in the
// |new_part|. The file will be split in chunks of |chunk_blocks| blocks each
// or treated as a single chunk if |chunk_blocks| is -1. The file data is
// stored in |new_part| in the blocks described by |new_extents| and, if it
// exists, the old version exists in |old_part| in the blocks described by
// |old_extents|. The operations added to |aops| reference the data blob
// in the file |data_fd|, which has length *data_file_size. *data_file_size is
// updated appropriately. Returns true on success.
bool DeltaReadFile(std::vector<AnnotatedOperation>* aops,
                   const std::string& old_part,
                   const std::string& new_part,
                   const std::vector<Extent>& old_extents,
                   const std::vector<Extent>& new_extents,
                   const std::string& name,
                   off_t chunk_blocks,
                   int data_fd,
                   off_t* data_file_size,
                   bool src_ops_allowed);

// Reads the blocks |old_extents| from |old_part| (if it exists) and the
// |new_extents| from |new_part| and determines the smallest way to encode
// this |new_extents| for the diff. It stores necessary data in |out_data| and
// fills in |out_op|. If there's no change in old and new files, it creates a
// MOVE operation. If there is a change, the smallest of REPLACE, REPLACE_BZ,
// or BSDIFF wins. |new_extents| must not be empty.
// If |src_ops_allowed| is true, it will emit SOURCE_COPY and SOURCE_BSDIFF
// operations instead of MOVE and BSDIFF, respectively.
// Returns true on success.
bool ReadExtentsToDiff(const std::string& old_part,
                       const std::string& new_part,
                       const std::vector<Extent>& old_extents,
                       const std::vector<Extent>& new_extents,
                       bool bsdiff_allowed,
                       chromeos::Blob* out_data,
                       DeltaArchiveManifest_InstallOperation* out_op,
                       bool src_ops_allowed);

// Runs the bsdiff tool on two files and returns the resulting delta in
// |out|. Returns true on success.
bool BsdiffFiles(const std::string& old_file,
                 const std::string& new_file,
                 chromeos::Blob* out);

// Returns true if |op| is a no-op operation that doesn't do any useful work
// (e.g., a move operation that copies blocks onto themselves).
bool IsNoopOperation(const DeltaArchiveManifest_InstallOperation& op);

// Filters all the operations that are no-op, maintaining the relative order
// of the rest of the operations.
void FilterNoopOperations(std::vector<AnnotatedOperation>* ops);

bool InitializePartitionInfo(const PartitionConfig& partition,
                             PartitionInfo* info);

// Compare two AnnotatedOperations by the start block of the first Extent in
// their destination extents.
bool CompareAopsByDestination(AnnotatedOperation first_aop,
                              AnnotatedOperation second_aop);

}  // namespace diff_utils

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_UTILS_H_
