// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_GENERATOR_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_GENERATOR_H_

#include <set>
#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/secure_blob.h>

#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/graph_types.h"
#include "update_engine/payload_generator/graph_utils.h"
#include "update_engine/payload_generator/operations_generator.h"
#include "update_engine/payload_generator/payload_generation_config.h"
#include "update_engine/update_metadata.pb.h"

// There is one function in DeltaDiffGenerator of importance to users
// of the class: GenerateDeltaUpdateFile(). Before calling it,
// the old and new images must be mounted. Call GenerateDeltaUpdateFile()
// with both the mount-points of the images in addition to the paths of
// the images (both old and new). A delta from old to new will be
// generated and stored in output_path.

namespace chromeos_update_engine {

extern const char* const kEmptyPath;
extern const size_t kBlockSize;
extern const size_t kRootFSPartitionSize;

class DeltaDiffGenerator : public OperationsGenerator {
 public:
  DeltaDiffGenerator() = default;

  // Represents a disk block on the install partition.
  struct Block {
    // During install, each block on the install partition will be written
    // and some may be read (in all likelihood, many will be read).
    // The reading and writing will be performed by InstallOperations,
    // each of which has a corresponding vertex in a graph.
    // A Block object tells which vertex will read or write this block
    // at install time.
    // Generally, there will be a vector of Block objects whose length
    // is the number of blocks on the install partition.
    Block() : reader(Vertex::kInvalidIndex), writer(Vertex::kInvalidIndex) {}
    Vertex::Index reader;
    Vertex::Index writer;
  };

  // These functions are public so that the unit tests can access them:

  // Generate the update payload operations for the kernel and rootfs using
  // SOURCE_* operations, used to generate deltas for the minor version
  // kSourceMinorPayloadVersion. This function will generate operations in the
  // rootfs that will read blocks from the source partition in random order and
  // write the new image on the target partition, also possibly in random order.
  // The rootfs operations are stored in |rootfs_ops| and should be executed in
  // that order. The kernel operations are stored in |kernel_ops|. All
  // the offsets in the operations reference the data written to |data_file_fd|.
  // The total amount of data written to that file is stored in
  // |data_file_size|.
  bool GenerateOperations(
      const PayloadGenerationConfig& config,
      int data_file_fd,
      off_t* data_file_size,
      std::vector<AnnotatedOperation>* rootfs_ops,
      std::vector<AnnotatedOperation>* kernel_ops) override;

  // For each regular file within new_root, creates a node in the graph,
  // determines the best way to compress it (REPLACE, REPLACE_BZ, COPY, BSDIFF),
  // and writes any necessary data to the end of data_fd.
  static bool DeltaReadFiles(Graph* graph,
                             std::vector<Block>* blocks,
                             const std::string& old_part,
                             const std::string& new_part,
                             const std::string& old_root,
                             const std::string& new_root,
                             off_t chunk_size,
                             int data_fd,
                             off_t* data_file_size,
                             bool src_ops_allowed);

  // For a given regular file which must exist at new_root + path, and
  // may exist at old_root + path, creates a new InstallOperation and
  // adds it to the graph. Also, populates the |blocks| array as
  // necessary, if |blocks| is non-null.  Also, writes the data
  // necessary to send the file down to the client into data_fd, which
  // has length *data_file_size. *data_file_size is updated
  // appropriately. If |existing_vertex| is no kInvalidIndex, use that
  // rather than allocating a new vertex. Returns true on success.
  static bool DeltaReadFile(Graph* graph,
                            Vertex::Index existing_vertex,
                            std::vector<Block>* blocks,
                            const std::string& old_part,
                            const std::string& new_part,
                            const std::string& old_root,
                            const std::string& new_root,
                            const std::string& path,
                            off_t chunk_offset,
                            off_t chunk_size,
                            int data_fd,
                            off_t* data_file_size,
                            bool src_ops_allowed);

  // Reads old_filename (if it exists) and a new_filename and determines
  // the smallest way to encode this file for the diff. It reads extents from
  // |old_part| and |new_part|. It stores necessary data in out_data and fills
  // in out_op. If there's no change in old and new files, it creates a MOVE
  // operation. If there is a change, or the old file doesn't exist,
  // the smallest of REPLACE, REPLACE_BZ, or BSDIFF wins.
  // new_filename must contain at least one byte.
  // |new_filename| is read starting at |chunk_offset|.
  // If |chunk_size| is not -1, only up to |chunk_size| bytes are diffed.
  // If |src_ops_allowed| is true, it will emit SOURCE_COPY and SOURCE_BSDIFF
  // operations instead of MOVE and BSDIFF, respectively.
  // Returns true on success.
  static bool ReadFileToDiff(const std::string& old_part,
                             const std::string& new_part,
                             off_t chunk_offset,
                             off_t chunk_size,
                             bool bsdiff_allowed,
                             chromeos::Blob* out_data,
                             DeltaArchiveManifest_InstallOperation* out_op,
                             bool gather_extents,
                             bool src_ops_allowed,
                             const std::string& old_filename,
                             const std::string& new_filename);

  // Delta compresses a kernel partition |new_kernel_part| with knowledge of the
  // old kernel partition |old_kernel_part|. If |old_kernel_part| is an empty
  // string, generates a full update of the partition.
  static bool DeltaCompressKernelPartition(
      const std::string& old_kernel_part,
      const std::string& new_kernel_part,
      std::vector<AnnotatedOperation>* ops,
      int blobs_fd,
      off_t* blobs_length,
      bool src_ops_allowed);

  // Reads blocks from image_path that are not yet marked as being written in
  // the blocks array. These blocks that remain are either unchanged files or
  // non-file-data blocks.  We compare each of them to the old image, and
  // compress the ones that changed into a single REPLACE_BZ operation. This
  // updates a newly created node in the graph to write these blocks and writes
  // the appropriate blob to blobs_fd. Reads and updates blobs_length.
  static bool ReadUnwrittenBlocks(
      const std::vector<Block>& blocks,
      int blobs_fd,
      off_t* blobs_length,
      const std::string& old_image_path,
      const std::string& new_image_path,
      Vertex* vertex,
      uint32_t minor_version);

  // Stores all Extents in 'extents' into 'out'.
  static void StoreExtents(const std::vector<Extent>& extents,
                           google::protobuf::RepeatedPtrField<Extent>* out);

  // Install operations in the manifest may reference data blobs, which
  // are in data_blobs_path. This function creates a new data blobs file
  // with the data blobs in the same order as the referencing install
  // operations in the manifest. E.g. if manifest[0] has a data blob
  // "X" at offset 1, manifest[1] has a data blob "Y" at offset 0,
  // and data_blobs_path's file contains "YX", new_data_blobs_path
  // will set to be a file that contains "XY".
  static bool ReorderDataBlobs(DeltaArchiveManifest* manifest,
                               const std::string& data_blobs_path,
                               const std::string& new_data_blobs_path);

  // Computes a SHA256 hash of the given buf and sets the hash value in the
  // operation so that update_engine could verify. This hash should be set
  // for all operations that have a non-zero data blob. One exception is the
  // dummy operation for signature blob because the contents of the signature
  // blob will not be available at payload creation time. So, update_engine will
  // gracefully ignore the dummy signature operation.
  static bool AddOperationHash(DeltaArchiveManifest_InstallOperation* op,
                               const chromeos::Blob& buf);


  // Returns true if |op| is a no-op operation that doesn't do any useful work
  // (e.g., a move operation that copies blocks onto themselves).
  static bool IsNoopOperation(const DeltaArchiveManifest_InstallOperation& op);

  // Filters all the operations that are no-op, maintaining the relative order
  // of the rest of the operations.
  static void FilterNoopOperations(std::vector<AnnotatedOperation>* ops);

  static bool InitializePartitionInfo(bool is_kernel,
                                      const std::string& partition,
                                      PartitionInfo* info);

  // Runs the bsdiff tool on two files and returns the resulting delta in
  // |out|. Returns true on success.
  static bool BsdiffFiles(const std::string& old_file,
                          const std::string& new_file,
                          chromeos::Blob* out);

  // Adds to |manifest| a dummy operation that points to a signature blob
  // located at the specified offset/length.
  static void AddSignatureOp(uint64_t signature_blob_offset,
                             uint64_t signature_blob_length,
                             DeltaArchiveManifest* manifest);

  // Takes a collection (vector or RepeatedPtrField) of Extent and
  // returns a vector of the blocks referenced, in order.
  template<typename T>
  static std::vector<uint64_t> ExpandExtents(const T& extents) {
    std::vector<uint64_t> ret;
    for (size_t i = 0, e = static_cast<size_t>(extents.size()); i != e; ++i) {
      const Extent extent = graph_utils::GetElement(extents, i);
      if (extent.start_block() == kSparseHole) {
        ret.resize(ret.size() + extent.num_blocks(), kSparseHole);
      } else {
        for (uint64_t block = extent.start_block();
             block < (extent.start_block() + extent.num_blocks()); block++) {
          ret.push_back(block);
        }
      }
    }
    return ret;
  }

  // Takes a vector of extents and removes extents that begin in a sparse hole.
  static void ClearSparseHoles(std::vector<Extent>* extents);

  // Takes a vector of extents and normalizes those extents. Expects the extents
  // to be sorted by start block. E.g. if |extents| is [(1, 2), (3, 5), (10, 2)]
  // then |extents| will be changed to [(1, 7), (10, 2)].
  static void NormalizeExtents(std::vector<Extent>* extents);

  // Takes a vector of AnnotatedOperations |aops| and fragments those operations
  // such that there is only one dst extent per operation. Sets |aops| to a
  // vector of the new fragmented operations. This is only called when delta
  // minor version is 2.
  // Currently, this only modifies SOURCE_COPY operations, but it will
  // eventually fragment all operations.
  static bool FragmentOperations(std::vector<AnnotatedOperation>* aops);

  // Takes an SOURCE_COPY install operation, |aop|, and adds one operation for
  // each dst extent in |aop| to |ops|. The new operations added to |ops| will
  // have only one dst extent. The src extents are split so the number of blocks
  // in the src and dst extents are equal.
  // E.g. we have a SOURCE_COPY operation:
  //   src extents: [(1, 3), (5, 1), (7, 1)], dst extents: [(2, 2), (6, 3)]
  // Then we will get 2 new operations:
  //   1. src extents: [(1, 2)], dst extents: [(2, 2)]
  //   2. src extents: [(3, 1),(5, 1),(7, 1)], dst extents: [(6, 3)]
  static bool SplitSourceCopy(const AnnotatedOperation& original_aop,
                              std::vector<AnnotatedOperation>* result_aops);

  // Takes a REPLACE operation, |aop|, and adds one operation for each dst
  // extent in |aop| to |ops|. The new operations added to |ops| will have only
  // one dst extent each.
  static bool SplitReplace(const AnnotatedOperation& original_aop,
                           std::vector<AnnotatedOperation>* result_aops);

 private:
  DISALLOW_COPY_AND_ASSIGN(DeltaDiffGenerator);
};

// This is the only function that external users of this module should call.
// The |config| describes the payload generation request, describing both
// old and new images for delta payloads and only the new image for full
// payloads.
// For delta payloads, the images should be already mounted read-only at
// the respective rootfs_mountpt.
// |private_key_path| points to a private key used to sign the update.
// Pass empty string to not sign the update.
// |output_path| is the filename where the delta update should be written.
// Returns true on success. Also writes the size of the metadata into
// |metadata_size|.
bool GenerateUpdatePayloadFile(const PayloadGenerationConfig& config,
                               const std::string& output_path,
                               const std::string& private_key_path,
                               uint64_t* metadata_size);


};  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_GENERATOR_H_
