// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_UTILS_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_UTILS_H_

#include <string>
#include <vector>

#include <chromeos/secure_blob.h>

#include "update_engine/payload_generator/annotated_operation.h"
#include "update_engine/payload_generator/filesystem_interface.h"
#include "update_engine/payload_generator/payload_generation_config.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

namespace diff_utils {

// Create operations in |aops| to produce all the files reported by |new_fs|,
// including all the blocks not reported by any file.
// It uses the files reported by |old_fs| and the data in |old_part| to
// determine the best way to compress the new files (REPLACE, REPLACE_BZ,
// COPY, BSDIFF) and writes any necessary data to the end of data_fd updating
// data_file_size accordingly.
bool DeltaReadFilesystem(std::vector<AnnotatedOperation>* aops,
                         const std::string& old_part,
                         const std::string& new_part,
                         FilesystemInterface* old_fs,
                         FilesystemInterface* new_fs,
                         off_t chunk_blocks,
                         int data_fd,
                         off_t* data_file_size,
                         bool skip_block_0,
                         bool src_ops_allowed);

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

// Delta compresses a kernel partition |new_kernel_part| with knowledge of the
// old kernel partition |old_kernel_part|. If |old_kernel_part| is an empty
// string, generates a full update of the partition. The size of the old and
// new kernel is passed in |old_kernel_size| and |new_kernel_size|. The
// operations used to generate the new kernel are stored in the |aops|
// vector, and the blob associated to those operations is written at the end
// of the |blobs_fd| file, adding to the value pointed by |blobs_length| the
// bytes written to |blobs_fd|.
bool DeltaCompressKernelPartition(
    const std::string& old_kernel_part,
    const std::string& new_kernel_part,
    uint64_t old_kernel_size,
    uint64_t new_kernel_size,
    uint64_t block_size,
    std::vector<AnnotatedOperation>* aops,
    int blobs_fd,
    off_t* blobs_length,
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

}  // namespace diff_utils

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_UTILS_H_
