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
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>

#include "update_engine/bzip.h"
#include "update_engine/utils.h"

using std::deque;
using std::shared_ptr;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

const size_t kDefaultFullChunkSize = 1024 * 1024;  // 1 MiB

// This class encapsulates a full update chunk processing thread. The processor
// reads a chunk of data from the input file descriptor and compresses it. The
// processor needs to be started through Start() then waited on through Wait().
class ChunkProcessor {
 public:
  // Read a chunk of |size| bytes from |fd| starting at offset |offset|.
  ChunkProcessor(int fd, off_t offset, size_t size)
      : thread_(nullptr),
        fd_(fd),
        offset_(offset),
        buffer_in_(size) {}
  ~ChunkProcessor() { Wait(); }

  off_t offset() const { return offset_; }
  const chromeos::Blob& buffer_in() const { return buffer_in_; }
  const chromeos::Blob& buffer_compressed() const { return buffer_compressed_; }

  // Starts the processor. Returns true on success, false on failure.
  bool Start();

  // Waits for the processor to complete. Returns true on success, false on
  // failure.
  bool Wait();

  bool ShouldCompress() const {
    return buffer_compressed_.size() < buffer_in_.size();
  }

 private:
  // Reads the input data into |buffer_in_| and compresses it into
  // |buffer_compressed_|. Returns true on success, false otherwise.
  bool ReadAndCompress();
  static gpointer ReadAndCompressThread(gpointer data);

  GThread* thread_;
  int fd_;
  off_t offset_;
  chromeos::Blob buffer_in_;
  chromeos::Blob buffer_compressed_;

  DISALLOW_COPY_AND_ASSIGN(ChunkProcessor);
};

bool ChunkProcessor::Start() {
  // g_thread_create is deprecated since glib 2.32. Use
  // g_thread_new instead.
  thread_ = g_thread_try_new("chunk_proc", ReadAndCompressThread, this,
                             nullptr);
  TEST_AND_RETURN_FALSE(thread_ != nullptr);
  return true;
}

bool ChunkProcessor::Wait() {
  if (!thread_) {
    return false;
  }
  gpointer result = g_thread_join(thread_);
  thread_ = nullptr;
  TEST_AND_RETURN_FALSE(result == this);
  return true;
}

gpointer ChunkProcessor::ReadAndCompressThread(gpointer data) {
  return reinterpret_cast<ChunkProcessor*>(data)->ReadAndCompress() ?
      data : nullptr;
}

bool ChunkProcessor::ReadAndCompress() {
  ssize_t bytes_read = -1;
  TEST_AND_RETURN_FALSE(utils::PReadAll(fd_,
                                        buffer_in_.data(),
                                        buffer_in_.size(),
                                        offset_,
                                        &bytes_read));
  TEST_AND_RETURN_FALSE(bytes_read == static_cast<ssize_t>(buffer_in_.size()));
  TEST_AND_RETURN_FALSE(BzipCompress(buffer_in_, &buffer_compressed_));
  return true;
}

}  // namespace

bool FullUpdateGenerator::GenerateOperations(
    const PayloadGenerationConfig& config,
    int fd,
    off_t* data_file_size,
    vector<AnnotatedOperation>* rootfs_ops,
    vector<AnnotatedOperation>* kernel_ops) {
  TEST_AND_RETURN_FALSE(config.Validate());
  rootfs_ops->clear();
  kernel_ops->clear();

  // FullUpdateGenerator requires a positive chunk_size, otherwise there will
  // be only one operation with the whole partition which should not be allowed.
  size_t full_chunk_size = kDefaultFullChunkSize;
  if (config.chunk_size >= 0) {
    full_chunk_size = config.chunk_size;
  } else {
    LOG(INFO) << "No chunk_size provided, using the default chunk_size for the "
              << "full operations: " << full_chunk_size << " bytes.";
  }
  TEST_AND_RETURN_FALSE(full_chunk_size > 0);
  TEST_AND_RETURN_FALSE(full_chunk_size % config.block_size == 0);

  size_t max_threads = std::max(sysconf(_SC_NPROCESSORS_ONLN), 4L);
  LOG(INFO) << "Max threads: " << max_threads;

  PartitionConfig partitions[] = { config.target.rootfs, config.target.kernel };

  for (int part_id = 0; part_id < 2; ++part_id) {
    const PartitionConfig& partition = partitions[part_id];
    LOG(INFO) << "compressing " << partition.path;
    int in_fd = open(partition.path.c_str(), O_RDONLY, 0);
    TEST_AND_RETURN_FALSE(in_fd >= 0);
    ScopedFdCloser in_fd_closer(&in_fd);
    deque<shared_ptr<ChunkProcessor>> threads;
    int last_progress_update = INT_MIN;
    size_t bytes_left = partition.size, counter = 0, offset = 0;
    while (bytes_left > 0 || !threads.empty()) {
      // Check and start new chunk processors if possible.
      while (threads.size() < max_threads && bytes_left > 0) {
        size_t this_chunk_bytes = std::min(bytes_left, full_chunk_size);
        shared_ptr<ChunkProcessor> processor(
            new ChunkProcessor(in_fd, offset, this_chunk_bytes));
        threads.push_back(processor);
        TEST_AND_RETURN_FALSE(processor->Start());
        bytes_left -= this_chunk_bytes;
        offset += this_chunk_bytes;
      }

      // Need to wait for a chunk processor to complete and process its output
      // before spawning new processors.
      shared_ptr<ChunkProcessor> processor = threads.front();
      threads.pop_front();
      TEST_AND_RETURN_FALSE(processor->Wait());

      DeltaArchiveManifest_InstallOperation* op = nullptr;
      if (part_id == 0) {
        rootfs_ops->emplace_back();
        rootfs_ops->back().name =
            base::StringPrintf("<rootfs-operation-%" PRIuS ">", counter++);
        op = &rootfs_ops->back().op;
      } else {
        kernel_ops->emplace_back();
        kernel_ops->back().name =
            base::StringPrintf("<kernel-operation-%" PRIuS ">", counter++);
        op = &kernel_ops->back().op;
      }

      const bool compress = processor->ShouldCompress();
      const chromeos::Blob& use_buf =
          compress ? processor->buffer_compressed() : processor->buffer_in();
      op->set_type(compress ?
                   DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ :
                   DeltaArchiveManifest_InstallOperation_Type_REPLACE);
      op->set_data_offset(*data_file_size);
      TEST_AND_RETURN_FALSE(utils::WriteAll(fd, use_buf.data(),
                                            use_buf.size()));
      *data_file_size += use_buf.size();
      op->set_data_length(use_buf.size());
      Extent* dst_extent = op->add_dst_extents();
      dst_extent->set_start_block(processor->offset() / config.block_size);
      dst_extent->set_num_blocks(full_chunk_size / config.block_size);

      int progress = static_cast<int>(
          (processor->offset() + processor->buffer_in().size()) * 100.0 /
          partition.size);
      if (last_progress_update < progress &&
          (last_progress_update + 10 <= progress || progress == 100)) {
        LOG(INFO) << progress << "% complete (output size: "
                  << *data_file_size << ")";
        last_progress_update = progress;
      }
    }
  }

  return true;
}

}  // namespace chromeos_update_engine
