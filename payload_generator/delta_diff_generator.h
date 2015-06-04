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
#include "update_engine/payload_generator/extent_utils.h"
#include "update_engine/payload_generator/filesystem_interface.h"
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

  // Create operations in |aops| to produce all the files reported by |new_fs|,
  // including all the blocks not reported by any file.
  // It uses the files reported by |old_fs| and the data in |old_part| to
  // determine the best way to compress the new files (REPLACE, REPLACE_BZ,
  // COPY, BSDIFF) and writes any necessary data to the end of data_fd updating
  // data_file_size accordingly.
  static bool DeltaReadFilesystem(std::vector<AnnotatedOperation>* aops,
                                  const std::string& old_part,
                                  const std::string& new_part,
                                  FilesystemInterface* old_fs,
                                  FilesystemInterface* new_fs,
                                  off_t chunk_blocks,
                                  int data_fd,
                                  off_t* data_file_size,
                                  bool src_ops_allowed);

  // For a given file |name| append operations to |aops| to produce it in the
  // |new_part|. The file will be split in chunks of |chunk_blocks| blocks each
  // or treated as a single chunk if |chunk_blocks| is -1. The file data is
  // stored in |new_part| in the blocks described by |new_extents| and, if it
  // exists, the old version exists in |old_part| in the blocks described by
  // |old_extents|. The operations added to |aops| reference the data blob
  // in the file |data_fd|, which has length *data_file_size. *data_file_size is
  // updated appropriately. Returns true on success.
  static bool DeltaReadFile(std::vector<AnnotatedOperation>* aops,
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
  static bool ReadExtentsToDiff(const std::string& old_part,
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
  static bool DeltaCompressKernelPartition(
      const std::string& old_kernel_part,
      const std::string& new_kernel_part,
      uint64_t old_kernel_size,
      uint64_t new_kernel_size,
      uint64_t block_size,
      std::vector<AnnotatedOperation>* aops,
      int blobs_fd,
      off_t* blobs_length,
      bool src_ops_allowed);

  // Stores all Extents in 'extents' into 'out'.
  static void StoreExtents(const std::vector<Extent>& extents,
                           google::protobuf::RepeatedPtrField<Extent>* out);

  // Stores all extents in |extents| into |out_vector|.
  static void ExtentsToVector(
    const google::protobuf::RepeatedPtrField<Extent>& extents,
    std::vector<Extent>* out_vector);

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
      const Extent extent = GetElement(extents, i);
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

  // Takes a vector of AnnotatedOperations |aops| and fragments those operations
  // such that there is only one dst extent per operation. Sets |aops| to a
  // vector of the new fragmented operations.
  static bool FragmentOperations(std::vector<AnnotatedOperation>* aops,
                                 const std::string& target_rootfs_part,
                                 int data_fd,
                                 off_t* data_file_size);

  // Takes a vector of AnnotatedOperations |aops| and sorts them by the first
  // start block in their destination extents. Sets |aops| to a vector of the
  // sorted operations.
  static void SortOperationsByDestination(
      std::vector<AnnotatedOperation>* aops);

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

  // Takes a REPLACE/REPLACE_BZ operation |aop|, and adds one operation for each
  // dst extent in |aop| to |ops|. The new operations added to |ops| will have
  // only one dst extent each, and may be either a REPLACE or REPLACE_BZ
  // depending on whether compression is advantageous.
  static bool SplitReplaceOrReplaceBz(
      const AnnotatedOperation& original_aop,
      std::vector<AnnotatedOperation>* result_aops,
      const std::string& target_part,
      int data_fd,
      off_t* data_file_size);

  // Takes a sorted (by first destination extent) vector of operations |aops|
  // and merges SOURCE_COPY, REPLACE, and REPLACE_BZ operations in that vector.
  // It will merge two operations if:
  //   - They are of the same type.
  //   - They are contiguous.
  //   - Their combined blocks do not exceed |chunk_size|.
  static bool MergeOperations(std::vector<AnnotatedOperation>* aops,
                              off_t chunk_size,
                              const std::string& target_part,
                              int data_fd,
                              off_t* data_file_size);

  // Takes a pointer to extents |extents| and extents |extents_to_add|, and
  // merges them by adding |extents_to_add| to |extents| and normalizing.
  static void ExtendExtents(
    google::protobuf::RepeatedPtrField<Extent>* extents,
    const google::protobuf::RepeatedPtrField<Extent>& extents_to_add);

 private:
  // Adds the data payload for a REPLACE/REPLACE_BZ operation |aop| by reading
  // its output extents from |target_part_path| and appending a corresponding
  // data blob to |data_fd|. The blob will be compressed if this is smaller than
  // the uncompressed form, and the operation type will be set accordingly.
  // |*data_file_size| will be updated as well. If the operation happens to have
  // the right type and already points to a data blob, we check whether its
  // content is identical to the new one, in which case nothing is written.
  static bool AddDataAndSetType(AnnotatedOperation* aop,
                                const std::string& target_part_path,
                                int data_fd,
                                off_t* data_file_size);

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
