// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/full_update_generator.h"

#include <fcntl.h>
#include <inttypes.h>

#include <algorithm>
#include <deque>
#include <memory>

#include <base/format_macros.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/synchronization/lock.h>
#include <base/threading/simple_thread.h>
#include <chromeos/secure_blob.h>

#include "update_engine/bzip.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

const size_t kDefaultFullChunkSize = 1024 * 1024;  // 1 MiB

// This class encapsulates a full update chunk processing thread work. The
// processor reads a chunk of data from the input file descriptor and compresses
// it. The processor will destroy itself when the work is done.
class ChunkProcessor : public base::DelegateSimpleThread::Delegate {
 public:
  // Read a chunk of |size| bytes from |fd| starting at offset |offset|.
  ChunkProcessor(int fd, off_t offset, size_t size,
                 BlobFileWriter* blob_file, AnnotatedOperation* aop)
    : fd_(fd),
      offset_(offset),
      size_(size),
      blob_file_(blob_file),
      aop_(aop) {}
  // We use a default move constructor since all the data members are POD types.
  ChunkProcessor(ChunkProcessor&&) = default;
  ~ChunkProcessor() override = default;

  // Overrides DelegateSimpleThread::Delegate.
  // Run() handles the read from |fd| in a thread-safe way, and stores the
  // new operation to generate the region starting at |offset| of size |size|
  // in the output operation |aop|. The associated blob data is stored in
  // |blob_fd| and |blob_file_size| is updated.
  void Run() override;

 private:
  bool ProcessChunk();

  // Stores the operation blob in the |blob_fd_| and updates the
  // |blob_file_size| accordingly.
  // This method is thread-safe since it uses a mutex to access the file.
  // Returns the data offset where the data was written to.
  off_t StoreBlob(const chromeos::Blob& blob);

  // Work parameters.
  int fd_;
  off_t offset_;
  size_t size_;
  BlobFileWriter* blob_file_;
  AnnotatedOperation* aop_;

  DISALLOW_COPY_AND_ASSIGN(ChunkProcessor);
};

void ChunkProcessor::Run() {
  if (!ProcessChunk()) {
    LOG(ERROR) << "Error processing region at " << offset_ << " of size "
               << size_;
  }
}

bool ChunkProcessor::ProcessChunk() {
  chromeos::Blob buffer_in_(size_);
  chromeos::Blob op_blob;
  ssize_t bytes_read = -1;
  TEST_AND_RETURN_FALSE(utils::PReadAll(fd_,
                                        buffer_in_.data(),
                                        buffer_in_.size(),
                                        offset_,
                                        &bytes_read));
  TEST_AND_RETURN_FALSE(bytes_read == static_cast<ssize_t>(size_));
  TEST_AND_RETURN_FALSE(BzipCompress(buffer_in_, &op_blob));

  DeltaArchiveManifest_InstallOperation_Type op_type =
      DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ;

  if (op_blob.size() >= buffer_in_.size()) {
    // A REPLACE is cheaper than a REPLACE_BZ in this case.
    op_blob = std::move(buffer_in_);
    op_type = DeltaArchiveManifest_InstallOperation_Type_REPLACE;
  }

  off_t op_offset = blob_file_->StoreBlob(op_blob);
  TEST_AND_RETURN_FALSE(op_offset >= 0);
  aop_->op.set_data_offset(op_offset);
  aop_->op.set_data_length(op_blob.size());
  aop_->op.set_type(op_type);
  return true;
}

}  // namespace

bool FullUpdateGenerator::GenerateOperations(
    const PayloadGenerationConfig& config,
    BlobFileWriter* blob_file,
    vector<AnnotatedOperation>* rootfs_ops,
    vector<AnnotatedOperation>* kernel_ops) {
  TEST_AND_RETURN_FALSE(config.Validate());

  // FullUpdateGenerator requires a positive chunk_size, otherwise there will
  // be only one operation with the whole partition which should not be allowed.
  // For performance reasons, we force a small default hard limit of 1 MiB. This
  // limit can be changed in the config, and we will use the smaller of the two
  // soft/hard limits.
  size_t full_chunk_size;
  if (config.hard_chunk_size >= 0) {
    full_chunk_size = std::min(static_cast<size_t>(config.hard_chunk_size),
                               config.soft_chunk_size);
  } else {
    full_chunk_size = std::min(kDefaultFullChunkSize, config.soft_chunk_size);
    LOG(INFO) << "No chunk_size provided, using the default chunk_size for the "
              << "full operations: " << full_chunk_size << " bytes.";
  }
  TEST_AND_RETURN_FALSE(full_chunk_size > 0);
  TEST_AND_RETURN_FALSE(full_chunk_size % config.block_size == 0);

  TEST_AND_RETURN_FALSE(GenerateOperationsForPartition(
      config.target.rootfs,
      config.block_size,
      full_chunk_size / config.block_size,
      blob_file,
      rootfs_ops));
  TEST_AND_RETURN_FALSE(GenerateOperationsForPartition(
      config.target.kernel,
      config.block_size,
      full_chunk_size / config.block_size,
      blob_file,
      kernel_ops));
  return true;
}

bool FullUpdateGenerator::GenerateOperationsForPartition(
    const PartitionConfig& new_part,
    size_t block_size,
    size_t chunk_blocks,
    BlobFileWriter* blob_file,
    vector<AnnotatedOperation>* aops) {
  size_t max_threads = std::max(sysconf(_SC_NPROCESSORS_ONLN), 4L);
  LOG(INFO) << "Compressing partition " << PartitionNameString(new_part.name)
            << " from " << new_part.path << " splitting in chunks of "
            << chunk_blocks << " blocks (" << block_size
            << " bytes each) using " << max_threads << " threads";

  int in_fd = open(new_part.path.c_str(), O_RDONLY, 0);
  TEST_AND_RETURN_FALSE(in_fd >= 0);
  ScopedFdCloser in_fd_closer(&in_fd);

  // We potentially have all the ChunkProcessors in memory but only
  // |max_threads| will actually hold a block in memory while we process.
  size_t partition_blocks = new_part.size / block_size;
  size_t num_chunks = (partition_blocks + chunk_blocks - 1) / chunk_blocks;
  aops->resize(num_chunks);
  vector<ChunkProcessor> chunk_processors;
  chunk_processors.reserve(num_chunks);
  blob_file->SetTotalBlobs(num_chunks);

  const string part_name_str = PartitionNameString(new_part.name);

  for (size_t i = 0; i < num_chunks; ++i) {
    size_t start_block = i * chunk_blocks;
    // The last chunk could be smaller.
    size_t num_blocks = std::min(chunk_blocks,
                                 partition_blocks - i * chunk_blocks);

    // Preset all the static information about the operations. The
    // ChunkProcessor will set the rest.
    AnnotatedOperation* aop = aops->data() + i;
    aop->name = base::StringPrintf("<%s-operation-%" PRIuS ">",
                                   part_name_str.c_str(), i);
    Extent* dst_extent = aop->op.add_dst_extents();
    dst_extent->set_start_block(start_block);
    dst_extent->set_num_blocks(num_blocks);

    chunk_processors.emplace_back(
        in_fd,
        static_cast<off_t>(start_block) * block_size,
        num_blocks * block_size,
        blob_file,
        aop);
  }

  // Thread pool used for worker threads.
  base::DelegateSimpleThreadPool thread_pool("full-update-generator",
                                             max_threads);
  thread_pool.Start();
  for (ChunkProcessor& processor : chunk_processors)
    thread_pool.AddWork(&processor);
  thread_pool.JoinAll();
  // All the operations must have a type set at this point. Otherwise, a
  // ChunkProcessor failed to complete.
  for (const AnnotatedOperation& aop : *aops) {
    if (!aop.op.has_type())
      return false;
  }
  return true;
}

}  // namespace chromeos_update_engine
