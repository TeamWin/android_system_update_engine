// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/delta_diff_generator.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>
#include <bzlib.h>

#include "update_engine/bzip.h"
#include "update_engine/delta_performer.h"
#include "update_engine/file_writer.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/extent_mapper.h"
#include "update_engine/payload_generator/filesystem_iterator.h"
#include "update_engine/payload_generator/full_update_generator.h"
#include "update_engine/payload_generator/graph_types.h"
#include "update_engine/payload_generator/graph_utils.h"
#include "update_engine/payload_generator/inplace_generator.h"
#include "update_engine/payload_generator/metadata.h"
#include "update_engine/payload_generator/payload_signer.h"
#include "update_engine/payload_verifier.h"
#include "update_engine/subprocess.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

using std::map;
using std::max;
using std::min;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

namespace {

const uint64_t kMajorVersionNumber = 1;

// The maximum destination size allowed for bsdiff. In general, bsdiff should
// work for arbitrary big files, but the payload generation and payload
// application requires a significant amount of RAM. We put a hard-limit of
// 200 MiB that should not affect any released board, but will limit the
// Chrome binary in ASan builders.
const off_t kMaxBsdiffDestinationSize = 200 * 1024 * 1024;  // bytes

static const char* kInstallOperationTypes[] = {
  "REPLACE",
  "REPLACE_BZ",
  "MOVE",
  "BSDIFF",
  "SOURCE_COPY",
  "SOURCE_BSDIFF"
};

}  // namespace

namespace chromeos_update_engine {

typedef DeltaDiffGenerator::Block Block;
typedef map<const DeltaArchiveManifest_InstallOperation*,
            string> OperationNameMap;

// bytes
const size_t kRootFSPartitionSize = static_cast<size_t>(2) * 1024 * 1024 * 1024;
const size_t kBlockSize = 4096;  // bytes
const char* const kEmptyPath = "";
const char* const kBsdiffPath = "bsdiff";

// Needed for testing purposes, in case we can't use actual filesystem objects.
// TODO(garnold) (chromium:331965) Replace this hack with a properly injected
// parameter in form of a mockable abstract class.
bool (*get_extents_with_chunk_func)(const string&, off_t, off_t,
                                    vector<Extent>*) =
    extent_mapper::ExtentsForFileChunkFibmap;

namespace {

bool IsSparseHole(const Extent &extent) {
  return (extent.start_block() == kSparseHole);
}

// Stores all the extents of |path| into |extents|. Returns true on success.
bool GatherExtents(const string& path,
                   off_t chunk_offset,
                   off_t chunk_size,
                   vector<Extent>* extents) {
  extents->clear();
  TEST_AND_RETURN_FALSE(
      get_extents_with_chunk_func(path, chunk_offset, chunk_size, extents));
  return true;
}

// Writes the uint64_t passed in in host-endian to the file as big-endian.
// Returns true on success.
bool WriteUint64AsBigEndian(FileWriter* writer, const uint64_t value) {
  uint64_t value_be = htobe64(value);
  TEST_AND_RETURN_FALSE(writer->Write(&value_be, sizeof(value_be)));
  return true;
}

// Adds each operation from |rootfs_ops| and |kernel_ops| to |out_manifest| in
// the order they come in those vectors. reports the operations names
void InstallOperationsToManifest(
    const vector<AnnotatedOperation>& rootfs_ops,
    const vector<AnnotatedOperation>& kernel_ops,
    DeltaArchiveManifest* out_manifest,
    OperationNameMap* out_op_name_map) {
  for (const AnnotatedOperation& aop : rootfs_ops) {
    if (DeltaDiffGenerator::IsNoopOperation(aop.op))
      continue;
    DeltaArchiveManifest_InstallOperation* new_op =
        out_manifest->add_install_operations();
    (*out_op_name_map)[new_op] = aop.name;
    *new_op = aop.op;
  }
  for (const AnnotatedOperation& aop : kernel_ops) {
    if (DeltaDiffGenerator::IsNoopOperation(aop.op))
      continue;
    DeltaArchiveManifest_InstallOperation* new_op =
        out_manifest->add_kernel_install_operations();
    (*out_op_name_map)[new_op] = aop.name;
    *new_op = aop.op;
  }
}

struct DeltaObject {
  DeltaObject(const string& in_name, const int in_type, const off_t in_size)
      : name(in_name),
        type(in_type),
        size(in_size) {}
  bool operator <(const DeltaObject& object) const {
    return (size != object.size) ? (size < object.size) : (name < object.name);
  }
  string name;
  int type;
  off_t size;
};

void ReportPayloadUsage(const DeltaArchiveManifest& manifest,
                        const int64_t manifest_metadata_size,
                        const OperationNameMap& op_name_map) {
  vector<DeltaObject> objects;
  off_t total_size = 0;

  // Rootfs install operations.
  for (int i = 0; i < manifest.install_operations_size(); ++i) {
    const DeltaArchiveManifest_InstallOperation& op =
        manifest.install_operations(i);
    objects.push_back(DeltaObject(op_name_map.find(&op)->second,
                                  op.type(),
                                  op.data_length()));
    total_size += op.data_length();
  }

  // Kernel install operations.
  for (int i = 0; i < manifest.kernel_install_operations_size(); ++i) {
    const DeltaArchiveManifest_InstallOperation& op =
        manifest.kernel_install_operations(i);
    objects.push_back(DeltaObject(base::StringPrintf("<kernel-operation-%d>",
                                                     i),
                                  op.type(),
                                  op.data_length()));
    total_size += op.data_length();
  }

  objects.push_back(DeltaObject("<manifest-metadata>",
                                -1,
                                manifest_metadata_size));
  total_size += manifest_metadata_size;

  std::sort(objects.begin(), objects.end());

  static const char kFormatString[] = "%6.2f%% %10jd %-10s %s\n";
  for (const DeltaObject& object : objects) {
    fprintf(stderr, kFormatString,
            object.size * 100.0 / total_size,
            static_cast<intmax_t>(object.size),
            object.type >= 0 ? kInstallOperationTypes[object.type] : "-",
            object.name.c_str());
  }
  fprintf(stderr, kFormatString,
          100.0, static_cast<intmax_t>(total_size), "", "<total>");
}

// Process a range of blocks from |range_start| to |range_end| in the extent at
// position |*idx_p| of |extents|. If |do_remove| is true, this range will be
// removed, which may cause the extent to be trimmed, split or removed entirely.
// The value of |*idx_p| is updated to point to the next extent to be processed.
// Returns true iff the next extent to process is a new or updated one.
bool ProcessExtentBlockRange(vector<Extent>* extents, size_t* idx_p,
                             const bool do_remove, uint64_t range_start,
                             uint64_t range_end) {
  size_t idx = *idx_p;
  uint64_t start_block = (*extents)[idx].start_block();
  uint64_t num_blocks = (*extents)[idx].num_blocks();
  uint64_t range_size = range_end - range_start;

  if (do_remove) {
    if (range_size == num_blocks) {
      // Remove the entire extent.
      extents->erase(extents->begin() + idx);
    } else if (range_end == num_blocks) {
      // Trim the end of the extent.
      (*extents)[idx].set_num_blocks(num_blocks - range_size);
      idx++;
    } else if (range_start == 0) {
      // Trim the head of the extent.
      (*extents)[idx].set_start_block(start_block + range_size);
      (*extents)[idx].set_num_blocks(num_blocks - range_size);
    } else {
      // Trim the middle, splitting the remainder into two parts.
      (*extents)[idx].set_num_blocks(range_start);
      Extent e;
      e.set_start_block(start_block + range_end);
      e.set_num_blocks(num_blocks - range_end);
      idx++;
      extents->insert(extents->begin() + idx, e);
    }
  } else if (range_end == num_blocks) {
    // Done with this extent.
    idx++;
  } else {
    return false;
  }

  *idx_p = idx;
  return true;
}

// Remove identical corresponding block ranges in |src_extents| and
// |dst_extents|. Used for preventing moving of blocks onto themselves during
// MOVE operations. The value of |total_bytes| indicates the actual length of
// content; this may be slightly less than the total size of blocks, in which
// case the last block is only partly occupied with data. Returns the total
// number of bytes removed.
size_t RemoveIdenticalBlockRanges(vector<Extent>* src_extents,
                                  vector<Extent>* dst_extents,
                                  const size_t total_bytes) {
  size_t src_idx = 0;
  size_t dst_idx = 0;
  uint64_t src_offset = 0, dst_offset = 0;
  bool new_src = true, new_dst = true;
  size_t removed_bytes = 0, nonfull_block_bytes;
  bool do_remove = false;
  while (src_idx < src_extents->size() && dst_idx < dst_extents->size()) {
    if (new_src) {
      src_offset = 0;
      new_src = false;
    }
    if (new_dst) {
      dst_offset = 0;
      new_dst = false;
    }

    do_remove = ((*src_extents)[src_idx].start_block() + src_offset ==
                 (*dst_extents)[dst_idx].start_block() + dst_offset);

    uint64_t src_num_blocks = (*src_extents)[src_idx].num_blocks();
    uint64_t dst_num_blocks = (*dst_extents)[dst_idx].num_blocks();
    uint64_t min_num_blocks = min(src_num_blocks - src_offset,
                                  dst_num_blocks - dst_offset);
    uint64_t prev_src_offset = src_offset;
    uint64_t prev_dst_offset = dst_offset;
    src_offset += min_num_blocks;
    dst_offset += min_num_blocks;

    new_src = ProcessExtentBlockRange(src_extents, &src_idx, do_remove,
                                      prev_src_offset, src_offset);
    new_dst = ProcessExtentBlockRange(dst_extents, &dst_idx, do_remove,
                                      prev_dst_offset, dst_offset);
    if (do_remove)
      removed_bytes += min_num_blocks * kBlockSize;
  }

  // If we removed the last block and this block is only partly used by file
  // content, deduct the unused portion from the total removed byte count.
  if (do_remove && (nonfull_block_bytes = total_bytes % kBlockSize))
    removed_bytes -= kBlockSize - nonfull_block_bytes;

  return removed_bytes;
}

}  // namespace

bool DeltaDiffGenerator::DeltaReadFiles(Graph* graph,
                                        vector<Block>* blocks,
                                        const string& old_part,
                                        const string& new_part,
                                        const string& old_root,
                                        const string& new_root,
                                        off_t chunk_size,
                                        int data_fd,
                                        off_t* data_file_size,
                                        bool src_ops_allowed) {
  set<ino_t> visited_inodes;
  set<ino_t> visited_src_inodes;
  for (FilesystemIterator fs_iter(new_root,
                                  set<string>{"/lost+found"});
       !fs_iter.IsEnd(); fs_iter.Increment()) {
    // We never diff symlinks (here, we check that dst file is not a symlink).
    if (!S_ISREG(fs_iter.GetStat().st_mode))
      continue;

    // Make sure we visit each inode only once.
    if (utils::SetContainsKey(visited_inodes, fs_iter.GetStat().st_ino))
      continue;
    visited_inodes.insert(fs_iter.GetStat().st_ino);
    off_t dst_size = fs_iter.GetFileSize();
    if (dst_size == 0)
      continue;

    LOG(INFO) << "Encoding file " << fs_iter.GetPartialPath();

    // We can't visit each dst image inode more than once, as that would
    // duplicate work. Here, we avoid visiting each source image inode
    // more than once. Technically, we could have multiple operations
    // that read the same blocks from the source image for diffing, but
    // we choose not to avoid complexity. Eventually we will move away
    // from using a graph/cycle detection/etc to generate diffs, and at that
    // time, it will be easy (non-complex) to have many operations read
    // from the same source blocks. At that time, this code can die. -adlr
    bool should_diff_from_source = false;
    string src_path = old_root + fs_iter.GetPartialPath();
    struct stat src_stbuf;
    // We never diff symlinks (here, we check that src file is not a symlink).
    if (0 == lstat(src_path.c_str(), &src_stbuf) &&
        S_ISREG(src_stbuf.st_mode)) {
      should_diff_from_source = !utils::SetContainsKey(visited_src_inodes,
                                                       src_stbuf.st_ino);
      visited_src_inodes.insert(src_stbuf.st_ino);
    }

    off_t size = chunk_size == -1 ? dst_size : chunk_size;
    off_t step = size;
    for (off_t offset = 0; offset < dst_size; offset += step) {
      if (offset + size >= dst_size) {
        size = -1;  // Read through the end of the file.
      }
      TEST_AND_RETURN_FALSE(DeltaDiffGenerator::DeltaReadFile(
          graph,
          Vertex::kInvalidIndex,
          blocks,
          old_part,
          new_part,
          (should_diff_from_source ? old_root : kEmptyPath),
          new_root,
          fs_iter.GetPartialPath(),
          offset,
          size,
          data_fd,
          data_file_size,
          src_ops_allowed));
    }
  }
  return true;
}

bool DeltaDiffGenerator::DeltaReadFile(Graph* graph,
                                       Vertex::Index existing_vertex,
                                       vector<Block>* blocks,
                                       const string& old_part,
                                       const string& new_part,
                                       const string& old_root,
                                       const string& new_root,
                                       const string& path,  // within new_root
                                       off_t chunk_offset,
                                       off_t chunk_size,
                                       int data_fd,
                                       off_t* data_file_size,
                                       bool src_ops_allowed) {
  chromeos::Blob data;
  DeltaArchiveManifest_InstallOperation operation;

  // If bsdiff breaks again, blacklist the problem file by using:
  //   bsdiff_allowed = (path != "/foo/bar")
  //
  // TODO(dgarrett): chromium-os:15274 connect this test to the command line.
  bool bsdiff_allowed = true;

  if (utils::FileSize(new_root + path) > kMaxBsdiffDestinationSize)
    bsdiff_allowed = false;

  if (!bsdiff_allowed)
    LOG(INFO) << "bsdiff blacklisting: " << path;

  string old_filename = (old_root == kEmptyPath) ? kEmptyPath : old_root + path;

  TEST_AND_RETURN_FALSE(DeltaDiffGenerator::ReadFileToDiff(old_part,
                                                           new_part,
                                                           chunk_offset,
                                                           chunk_size,
                                                           bsdiff_allowed,
                                                           &data,
                                                           &operation,
                                                           true,
                                                           src_ops_allowed,
                                                           old_filename,
                                                           new_root + path));

  // Check if the operation writes nothing.
  if (operation.dst_extents_size() == 0) {
    if (operation.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE) {
      LOG(INFO) << "Empty MOVE operation ("
                << new_root + path << "), skipping";
      return true;
    } else {
      LOG(ERROR) << "Empty non-MOVE operation";
      return false;
    }
  }

  // Write the data
  if (operation.type() != DeltaArchiveManifest_InstallOperation_Type_MOVE &&
      operation.type() !=
          DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY) {
    operation.set_data_offset(*data_file_size);
    operation.set_data_length(data.size());
  }

  TEST_AND_RETURN_FALSE(utils::WriteAll(data_fd, data.data(), data.size()));
  *data_file_size += data.size();

  // Now, insert into graph and blocks vector
  Vertex::Index vertex = existing_vertex;
  if (vertex == Vertex::kInvalidIndex) {
    graph->emplace_back();
    vertex = graph->size() - 1;
  }
  (*graph)[vertex].op = operation;
  CHECK((*graph)[vertex].op.has_type());
  (*graph)[vertex].file_name = path;
  (*graph)[vertex].chunk_offset = chunk_offset;
  (*graph)[vertex].chunk_size = chunk_size;

  if (blocks)
    TEST_AND_RETURN_FALSE(InplaceGenerator::AddInstallOpToBlocksVector(
        (*graph)[vertex].op,
        *graph,
        vertex,
        blocks));
  return true;
}

bool DeltaDiffGenerator::ReadFileToDiff(
    const string& old_part,
    const string& new_part,
    off_t chunk_offset,
    off_t chunk_size,
    bool bsdiff_allowed,
    chromeos::Blob* out_data,
    DeltaArchiveManifest_InstallOperation* out_op,
    bool gather_extents,
    bool src_ops_allowed,
    const string& old_filename,
    const string& new_filename) {

  // Do we have an original file to consider?
  off_t old_size = 0;
  bool original = !old_filename.empty();
  if (original && (old_size = utils::FileSize(old_filename)) < 0) {
    // If stat-ing the old file fails, it should be because it doesn't exist.
    TEST_AND_RETURN_FALSE(!utils::FileExists(old_filename.c_str()));
    original = false;
  }

  DeltaArchiveManifest_InstallOperation operation;
  vector<Extent> src_extents, dst_extents;
  // Gather source extents if we have an original file.
  if (original) {
    if (gather_extents) {
      TEST_AND_RETURN_FALSE(
          GatherExtents(old_filename, chunk_offset, chunk_size, &src_extents));
      ClearSparseHoles(&src_extents);
      if (src_extents.size() == 0) {
        // Reading from sparse hole, do nothing.
        operation.set_type(DeltaArchiveManifest_InstallOperation_Type_MOVE);
        *out_op = operation;
        return true;
      }
    } else {
      // We have a kernel, so make one extent to cover it all.
      Extent* src_extent = operation.add_src_extents();
      src_extent->set_start_block(0);
      src_extent->set_num_blocks(
          (utils::FileSize(old_filename) + (kBlockSize - 1)) / kBlockSize);
      src_extents.push_back(*src_extent);
    }
  }

  // Gather destination extents.
  if (gather_extents) {
    TEST_AND_RETURN_FALSE(
        GatherExtents(new_filename, chunk_offset, chunk_size, &dst_extents));
    ClearSparseHoles(&dst_extents);
    if (dst_extents.size() == 0) {
      // Make an empty move operation.
      operation.set_type(DeltaArchiveManifest_InstallOperation_Type_MOVE);
      *out_op = operation;
      return true;
    }
  } else {
    Extent* dst_extent = operation.add_dst_extents();
    dst_extent->set_start_block(0);
    dst_extent->set_num_blocks(
        (utils::FileSize(new_filename) + (kBlockSize - 1)) / kBlockSize);
    dst_extents.push_back(*dst_extent);
  }

  // Figure out how many blocks we need to write to dst_extents.
  uint64_t blocks_to_write = 0;
  for (uint32_t i = 0; i < dst_extents.size(); i++)
    blocks_to_write += dst_extents[i].num_blocks();

  // Figure out how many blocks we need to read to src_extents.
  uint64_t blocks_to_read = 0;
  for (uint32_t i = 0; i < src_extents.size(); i++)
    blocks_to_read += src_extents[i].num_blocks();

  // Read in bytes from new data.
  chromeos::Blob new_data;
  TEST_AND_RETURN_FALSE(utils::ReadExtents(new_part,
                                           &dst_extents,
                                           &new_data,
                                           kBlockSize * blocks_to_write,
                                           kBlockSize));

  TEST_AND_RETURN_FALSE(!new_data.empty());
  TEST_AND_RETURN_FALSE(chunk_size == -1 ||
                        static_cast<off_t>(new_data.size()) <= chunk_size);

  chromeos::Blob new_data_bz;
  TEST_AND_RETURN_FALSE(BzipCompress(new_data, &new_data_bz));
  CHECK(!new_data_bz.empty());
  chromeos::Blob data;  // Data blob that will be written to delta file.

  size_t current_best_size = 0;
  if (new_data.size() <= new_data_bz.size()) {
    operation.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE);
    current_best_size = new_data.size();
    data = new_data;
  } else {
    operation.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
    current_best_size = new_data_bz.size();
    data = new_data_bz;
  }
  chromeos::Blob old_data;
  if (original) {
    // Read old data.
    TEST_AND_RETURN_FALSE(
        utils::ReadExtents(old_part, &src_extents, &old_data,
                           kBlockSize * blocks_to_read, kBlockSize));
    if (old_data == new_data) {
      // No change in data.
      if (src_ops_allowed) {
        operation.set_type(
            DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY);
      } else {
        operation.set_type(DeltaArchiveManifest_InstallOperation_Type_MOVE);
      }
      current_best_size = 0;
      data.clear();
    } else if (!old_data.empty() && bsdiff_allowed) {
      // If the source file is considered bsdiff safe (no bsdiff bugs
      // triggered), see if BSDIFF encoding is smaller.
      base::FilePath old_chunk;
      TEST_AND_RETURN_FALSE(base::CreateTemporaryFile(&old_chunk));
      ScopedPathUnlinker old_unlinker(old_chunk.value());
      TEST_AND_RETURN_FALSE(
          utils::WriteFile(old_chunk.value().c_str(),
                           old_data.data(), old_data.size()));
      base::FilePath new_chunk;
      TEST_AND_RETURN_FALSE(base::CreateTemporaryFile(&new_chunk));
      ScopedPathUnlinker new_unlinker(new_chunk.value());
      TEST_AND_RETURN_FALSE(
          utils::WriteFile(new_chunk.value().c_str(),
                           new_data.data(), new_data.size()));

      chromeos::Blob bsdiff_delta;
      TEST_AND_RETURN_FALSE(
          BsdiffFiles(old_chunk.value(), new_chunk.value(), &bsdiff_delta));
      CHECK_GT(bsdiff_delta.size(), static_cast<chromeos::Blob::size_type>(0));
      if (bsdiff_delta.size() < current_best_size) {
        if (src_ops_allowed) {
          operation.set_type(
              DeltaArchiveManifest_InstallOperation_Type_SOURCE_BSDIFF);
        } else {
          operation.set_type(DeltaArchiveManifest_InstallOperation_Type_BSDIFF);
        }
        current_best_size = bsdiff_delta.size();
        data = bsdiff_delta;
      }
    }
  }

  operation.set_src_length(old_data.size());
  operation.set_dst_length(new_data.size());

  // Set parameters of the operations
  CHECK_EQ(data.size(), current_best_size);

  if (gather_extents) {
    // Remove identical src/dst block ranges in MOVE operations.
    if (operation.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE) {
      size_t removed_bytes = RemoveIdenticalBlockRanges(
          &src_extents, &dst_extents, new_data.size());

      // Adjust the file length field accordingly.
      if (removed_bytes) {
        operation.set_src_length(old_data.size() - removed_bytes);
        operation.set_dst_length(new_data.size() - removed_bytes);
      }
    }

    // Embed extents in the operation.
    StoreExtents(src_extents, operation.mutable_src_extents());
    StoreExtents(dst_extents, operation.mutable_dst_extents());
  }

  // Replace operations should not have source extents.
  if (operation.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE ||
      operation.type() ==
          DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ) {
    operation.clear_src_extents();
    operation.clear_src_length();
  }

  out_data->swap(data);
  *out_op = operation;

  return true;
}

bool DeltaDiffGenerator::DeltaCompressKernelPartition(
    const string& old_kernel_part,
    const string& new_kernel_part,
    vector<AnnotatedOperation>* kernel_ops,
    int blobs_fd,
    off_t* blobs_length,
    bool src_ops_allowed) {
  LOG(INFO) << "Delta compressing kernel partition...";
  LOG_IF(INFO, old_kernel_part.empty()) << "Generating full kernel update...";

  DeltaArchiveManifest_InstallOperation op;
  chromeos::Blob data;
  TEST_AND_RETURN_FALSE(
      ReadFileToDiff(old_kernel_part,
                     new_kernel_part,
                     0,  // chunk_offset
                     -1,  // chunk_size
                     true,  // bsdiff_allowed
                     &data,
                     &op,
                     false,  // gather_extents
                     src_ops_allowed,
                     old_kernel_part,  // Doesn't matter, kernel has no files.
                     new_kernel_part));

  // Check if the operation writes nothing.
  if (op.dst_extents_size() == 0) {
    if (op.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE) {
      LOG(INFO) << "Empty MOVE operation, nothing to do.";
      return true;
    } else {
      LOG(ERROR) << "Empty non-MOVE operation";
      return false;
    }
  }

  // Write the data.
  if (op.type() != DeltaArchiveManifest_InstallOperation_Type_MOVE &&
      op.type() != DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY) {
    op.set_data_offset(*blobs_length);
    op.set_data_length(data.size());
  }

  // Add the new install operation.
  kernel_ops->clear();
  kernel_ops->emplace_back();
  kernel_ops->back().op = op;
  kernel_ops->back().name = "<kernel-delta-operation>";

  TEST_AND_RETURN_FALSE(utils::WriteAll(blobs_fd, data.data(), data.size()));
  *blobs_length += data.size();

  LOG(INFO) << "Done delta compressing kernel partition: "
            << kInstallOperationTypes[op.type()];
  return true;
}

// TODO(deymo): Replace Vertex with AnnotatedOperation. This requires to move
// out the code that adds the reader dependencies on the new vertex.
bool DeltaDiffGenerator::ReadUnwrittenBlocks(
    const vector<Block>& blocks,
    int blobs_fd,
    off_t* blobs_length,
    const string& old_image_path,
    const string& new_image_path,
    Vertex* vertex,
    uint32_t minor_version) {
  vertex->file_name = "<rootfs-non-file-data>";

  DeltaArchiveManifest_InstallOperation* out_op = &vertex->op;
  int new_image_fd = open(new_image_path.c_str(), O_RDONLY, 000);
  TEST_AND_RETURN_FALSE_ERRNO(new_image_fd >= 0);
  ScopedFdCloser new_image_fd_closer(&new_image_fd);
  int old_image_fd = open(old_image_path.c_str(), O_RDONLY, 000);
  TEST_AND_RETURN_FALSE_ERRNO(old_image_fd >= 0);
  ScopedFdCloser old_image_fd_closer(&old_image_fd);

  string temp_file_path;
  TEST_AND_RETURN_FALSE(utils::MakeTempFile("CrAU_temp_data.XXXXXX",
                                            &temp_file_path,
                                            nullptr));

  FILE* file = fopen(temp_file_path.c_str(), "w");
  TEST_AND_RETURN_FALSE(file);
  int err = BZ_OK;

  BZFILE* bz_file = BZ2_bzWriteOpen(&err,
                                    file,
                                    9,  // max compression
                                    0,  // verbosity
                                    0);  // default work factor
  TEST_AND_RETURN_FALSE(err == BZ_OK);

  vector<Extent> extents;
  vector<Block>::size_type block_count = 0;

  LOG(INFO) << "Appending unwritten blocks to extents";
  for (vector<Block>::size_type i = 0; i < blocks.size(); i++) {
    if (blocks[i].writer != Vertex::kInvalidIndex)
      continue;
    graph_utils::AppendBlockToExtents(&extents, i);
    block_count++;
  }

  // Code will handle buffers of any size that's a multiple of kBlockSize,
  // so we arbitrarily set it to 1024 * kBlockSize.
  chromeos::Blob new_buf(1024 * kBlockSize);
  chromeos::Blob old_buf(1024 * kBlockSize);

  LOG(INFO) << "Scanning " << block_count << " unwritten blocks";
  vector<Extent> changed_extents;
  vector<Block>::size_type changed_block_count = 0;
  vector<Block>::size_type blocks_copied_count = 0;

  // For each extent in extents, write the unchanged blocks into BZ2_bzWrite,
  // which sends it to an output file.  We use the temporary buffers to hold the
  // old and new data, which may be smaller than the extent, so in that case we
  // have to loop to get the extent's data (that's the inner while loop).
  for (const Extent& extent : extents) {
    vector<Block>::size_type blocks_read = 0;
    float printed_progress = -1;
    while (blocks_read < extent.num_blocks()) {
      const uint64_t copy_first_block = extent.start_block() + blocks_read;
      const int copy_block_cnt =
          min(new_buf.size() / kBlockSize,
              static_cast<chromeos::Blob::size_type>(
                  extent.num_blocks() - blocks_read));
      const size_t count = copy_block_cnt * kBlockSize;
      const off_t offset = copy_first_block * kBlockSize;
      ssize_t rc = pread(new_image_fd, new_buf.data(), count, offset);
      TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
      TEST_AND_RETURN_FALSE(static_cast<size_t>(rc) == count);

      rc = pread(old_image_fd, old_buf.data(), count, offset);
      TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
      TEST_AND_RETURN_FALSE(static_cast<size_t>(rc) == count);

      // Compare each block in the buffer to its counterpart in the old image
      // and only compress it if its content has changed.
      int buf_offset = 0;
      for (int i = 0; i < copy_block_cnt; ++i) {
        int buf_end_offset = buf_offset + kBlockSize;
        if (minor_version == kSourceMinorPayloadVersion ||
            !std::equal(new_buf.begin() + buf_offset,
                        new_buf.begin() + buf_end_offset,
                        old_buf.begin() + buf_offset)) {
          BZ2_bzWrite(&err, bz_file, &new_buf[buf_offset], kBlockSize);
          TEST_AND_RETURN_FALSE(err == BZ_OK);
          const uint64_t block_idx = copy_first_block + i;
          if (blocks[block_idx].reader != Vertex::kInvalidIndex) {
            graph_utils::AddReadBeforeDep(vertex, blocks[block_idx].reader,
                                          block_idx);
          }
          graph_utils::AppendBlockToExtents(&changed_extents, block_idx);
          changed_block_count++;
        }
        buf_offset = buf_end_offset;
      }

      blocks_read += copy_block_cnt;
      blocks_copied_count += copy_block_cnt;
      float current_progress =
          static_cast<float>(blocks_copied_count) / block_count;
      if (printed_progress + 0.1 < current_progress ||
          blocks_copied_count == block_count) {
        LOG(INFO) << "progress: " << current_progress;
        printed_progress = current_progress;
      }
    }
  }
  BZ2_bzWriteClose(&err, bz_file, 0, nullptr, nullptr);
  TEST_AND_RETURN_FALSE(err == BZ_OK);
  bz_file = nullptr;
  TEST_AND_RETURN_FALSE_ERRNO(0 == fclose(file));
  file = nullptr;

  LOG(INFO) << "Compressed " << changed_block_count << " blocks ("
            << block_count - changed_block_count << " blocks unchanged)";
  chromeos::Blob compressed_data;
  if (changed_block_count > 0) {
    LOG(INFO) << "Reading compressed data off disk";
    TEST_AND_RETURN_FALSE(utils::ReadFile(temp_file_path, &compressed_data));
  }
  TEST_AND_RETURN_FALSE(unlink(temp_file_path.c_str()) == 0);

  // Add node to graph to write these blocks
  out_op->set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
  out_op->set_data_offset(*blobs_length);
  out_op->set_data_length(compressed_data.size());
  LOG(INFO) << "Rootfs non-data blocks compressed take up "
            << compressed_data.size();
  *blobs_length += compressed_data.size();
  out_op->set_dst_length(kBlockSize * changed_block_count);
  DeltaDiffGenerator::StoreExtents(changed_extents,
                                   out_op->mutable_dst_extents());

  TEST_AND_RETURN_FALSE(utils::WriteAll(blobs_fd,
                                        compressed_data.data(),
                                        compressed_data.size()));
  LOG(INFO) << "Done processing unwritten blocks";
  return true;
}

bool DeltaDiffGenerator::InitializePartitionInfo(bool is_kernel,
                                                 const string& partition,
                                                 PartitionInfo* info) {
  int64_t size = 0;
  if (is_kernel) {
    size = utils::FileSize(partition);
  } else {
    int block_count = 0, block_size = 0;
    TEST_AND_RETURN_FALSE(utils::GetFilesystemSize(partition,
                                                   &block_count,
                                                   &block_size));
    size = static_cast<int64_t>(block_count) * block_size;
  }
  TEST_AND_RETURN_FALSE(size > 0);
  info->set_size(size);
  OmahaHashCalculator hasher;
  TEST_AND_RETURN_FALSE(hasher.UpdateFile(partition, size) == size);
  TEST_AND_RETURN_FALSE(hasher.Finalize());
  const chromeos::Blob& hash = hasher.raw_hash();
  info->set_hash(hash.data(), hash.size());
  LOG(INFO) << partition << ": size=" << size << " hash=" << hasher.hash();
  return true;
}

bool InitializePartitionInfos(const PayloadGenerationConfig& config,
                              DeltaArchiveManifest* manifest) {
  if (!config.source.kernel_part.empty()) {
    TEST_AND_RETURN_FALSE(DeltaDiffGenerator::InitializePartitionInfo(
        true,
        config.source.kernel_part,
        manifest->mutable_old_kernel_info()));
  }
  TEST_AND_RETURN_FALSE(DeltaDiffGenerator::InitializePartitionInfo(
      true,
      config.target.kernel_part,
      manifest->mutable_new_kernel_info()));
  if (!config.source.rootfs_part.empty()) {
    TEST_AND_RETURN_FALSE(DeltaDiffGenerator::InitializePartitionInfo(
        false,
        config.source.rootfs_part,
        manifest->mutable_old_rootfs_info()));
  }
  TEST_AND_RETURN_FALSE(DeltaDiffGenerator::InitializePartitionInfo(
      false,
      config.target.rootfs_part,
      manifest->mutable_new_rootfs_info()));
  return true;
}

// Stores all Extents in 'extents' into 'out'.
void DeltaDiffGenerator::StoreExtents(
    const vector<Extent>& extents,
    google::protobuf::RepeatedPtrField<Extent>* out) {
  for (const Extent& extent : extents) {
    Extent* new_extent = out->Add();
    *new_extent = extent;
  }
}

// Returns true if |op| is a no-op operation that doesn't do any useful work
// (e.g., a move operation that copies blocks onto themselves).
bool DeltaDiffGenerator::IsNoopOperation(
    const DeltaArchiveManifest_InstallOperation& op) {
  return (op.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE &&
          ExpandExtents(op.src_extents()) == ExpandExtents(op.dst_extents()));
}

void DeltaDiffGenerator::FilterNoopOperations(vector<AnnotatedOperation>* ops) {
  ops->erase(
      std::remove_if(
          ops->begin(), ops->end(),
          [](const AnnotatedOperation& aop){return IsNoopOperation(aop.op);}),
      ops->end());
}

bool DeltaDiffGenerator::ReorderDataBlobs(
    DeltaArchiveManifest* manifest,
    const string& data_blobs_path,
    const string& new_data_blobs_path) {
  int in_fd = open(data_blobs_path.c_str(), O_RDONLY, 0);
  TEST_AND_RETURN_FALSE_ERRNO(in_fd >= 0);
  ScopedFdCloser in_fd_closer(&in_fd);

  DirectFileWriter writer;
  TEST_AND_RETURN_FALSE(
      writer.Open(new_data_blobs_path.c_str(),
                  O_WRONLY | O_TRUNC | O_CREAT,
                  0644) == 0);
  ScopedFileWriterCloser writer_closer(&writer);
  uint64_t out_file_size = 0;

  for (int i = 0; i < (manifest->install_operations_size() +
                       manifest->kernel_install_operations_size()); i++) {
    DeltaArchiveManifest_InstallOperation* op = nullptr;
    if (i < manifest->install_operations_size()) {
      op = manifest->mutable_install_operations(i);
    } else {
      op = manifest->mutable_kernel_install_operations(
          i - manifest->install_operations_size());
    }
    if (!op->has_data_offset())
      continue;
    CHECK(op->has_data_length());
    chromeos::Blob buf(op->data_length());
    ssize_t rc = pread(in_fd, buf.data(), buf.size(), op->data_offset());
    TEST_AND_RETURN_FALSE(rc == static_cast<ssize_t>(buf.size()));

    // Add the hash of the data blobs for this operation
    TEST_AND_RETURN_FALSE(AddOperationHash(op, buf));

    op->set_data_offset(out_file_size);
    TEST_AND_RETURN_FALSE(writer.Write(buf.data(), buf.size()));
    out_file_size += buf.size();
  }
  return true;
}

bool DeltaDiffGenerator::AddOperationHash(
    DeltaArchiveManifest_InstallOperation* op,
    const chromeos::Blob& buf) {
  OmahaHashCalculator hasher;

  TEST_AND_RETURN_FALSE(hasher.Update(buf.data(), buf.size()));
  TEST_AND_RETURN_FALSE(hasher.Finalize());

  const chromeos::Blob& hash = hasher.raw_hash();
  op->set_data_sha256_hash(hash.data(), hash.size());
  return true;
}

bool DeltaDiffGenerator::GenerateOperations(
    const PayloadGenerationConfig& config,
    int data_file_fd,
    off_t* data_file_size,
    vector<AnnotatedOperation>* rootfs_ops,
    vector<AnnotatedOperation>* kernel_ops) {
  // List of blocks in the target partition, with the operation that needs to
  // write it and the operation that needs to read it. This is used here to
  // keep track of the blocks that no operation is writing it.
  vector<Block> blocks(config.target.rootfs_size / config.block_size);

  // TODO(deymo): DeltaReadFiles() should not use a graph to generate the
  // operations, either in the in-place or source uprate. Split out the
  // graph dependency generation.
  Graph graph;
  TEST_AND_RETURN_FALSE(DeltaReadFiles(&graph,
                                       &blocks,
                                       config.source.rootfs_part,
                                       config.target.rootfs_part,
                                       config.source.rootfs_mountpt,
                                       config.target.rootfs_mountpt,
                                       config.chunk_size,
                                       data_file_fd,
                                       data_file_size,
                                       true));  // src_ops_allowed
  rootfs_ops->clear();
  for (const Vertex& v : graph) {
    rootfs_ops->emplace_back();
    AnnotatedOperation& aop = rootfs_ops->back();
    aop.op = v.op;
    aop.SetNameFromFileAndChunk(v.file_name, v.chunk_offset, v.chunk_size);
  }

  LOG(INFO) << "done reading normal files";

  // Read kernel partition
  TEST_AND_RETURN_FALSE(
      DeltaCompressKernelPartition(config.source.kernel_part,
                                   config.target.kernel_part,
                                   kernel_ops,
                                   data_file_fd,
                                   data_file_size,
                                   true));  // src_ops_allowed
  LOG(INFO) << "done reading kernel";

  Vertex unwritten_vertex;
  TEST_AND_RETURN_FALSE(ReadUnwrittenBlocks(blocks,
                                            data_file_fd,
                                            data_file_size,
                                            config.source.rootfs_part,
                                            config.target.rootfs_part,
                                            &unwritten_vertex,
                                            config.minor_version));
  if (unwritten_vertex.op.data_length() == 0) {
    LOG(INFO) << "No unwritten blocks to write, omitting operation";
  } else {
    rootfs_ops->emplace_back();
    rootfs_ops->back().op = unwritten_vertex.op;
    rootfs_ops->back().name = unwritten_vertex.file_name;
  }
  return true;
}

bool GenerateUpdatePayloadFile(
    const PayloadGenerationConfig& config,
    const string& output_path,
    const string& private_key_path,
    uint64_t* metadata_size) {
  if (config.is_delta) {
    LOG_IF(WARNING, config.source.rootfs_size != config.target.rootfs_size)
        << "Old and new images have different block counts.";
    // TODO(deymo): Our tools only support growing the filesystem size during
    // an update. Remove this check when that's fixed. crbug.com/192136
    LOG_IF(FATAL, config.source.rootfs_size > config.target.rootfs_size)
        << "Shirking the rootfs size is not supported at the moment.";
  }

  // Sanity checks for the partition size.
  LOG(INFO) << "Rootfs partition size: " << config.rootfs_partition_size;
  LOG(INFO) << "Actual filesystem size: " << config.target.rootfs_size;

  LOG(INFO) << "Invalid block index: " << Vertex::kInvalidIndex;
  LOG(INFO) << "Block count: "
            << config.target.rootfs_size / config.block_size;

  const string kTempFileTemplate("CrAU_temp_data.XXXXXX");
  string temp_file_path;
  unique_ptr<ScopedPathUnlinker> temp_file_unlinker;
  off_t data_file_size = 0;

  LOG(INFO) << "Reading files...";

  // Create empty protobuf Manifest object
  DeltaArchiveManifest manifest;
  manifest.set_minor_version(config.minor_version);

  vector<AnnotatedOperation> rootfs_ops;
  vector<AnnotatedOperation> kernel_ops;

  // Select payload generation strategy based on the config.
  unique_ptr<OperationsGenerator> strategy;
  if (config.is_delta) {
    // We don't efficiently support deltas on squashfs. For now, we will
    // produce full operations in that case.
    if (utils::IsSquashfsFilesystem(config.target.rootfs_part)) {
      LOG(INFO) << "Using generator FullUpdateGenerator::Run for squashfs "
                   "deltas";
      strategy.reset(new FullUpdateGenerator());
    } else if (utils::IsExtFilesystem(config.target.rootfs_part)) {
      // Delta update (with possibly a full kernel update).
      if (config.minor_version == kInPlaceMinorPayloadVersion) {
        LOG(INFO) << "Using generator InplaceGenerator::GenerateInplaceDelta";
        strategy.reset(new InplaceGenerator());
      } else if (config.minor_version == kSourceMinorPayloadVersion) {
        LOG(INFO) << "Using generator DeltaDiffGenerator::GenerateSourceDelta";
        strategy.reset(new DeltaDiffGenerator());
      } else {
        LOG(ERROR) << "Unsupported minor version given for delta payload: "
                   << config.minor_version;
        return false;
      }
    } else {
      LOG(ERROR) << "Unsupported filesystem for delta payload in "
                 << config.target.rootfs_part;
      return false;
    }
  } else {
    // Full update.
    LOG(INFO) << "Using generator FullUpdateGenerator::Run";
    strategy.reset(new FullUpdateGenerator());
  }

  {
    int data_file_fd;
    TEST_AND_RETURN_FALSE(
        utils::MakeTempFile(kTempFileTemplate, &temp_file_path, &data_file_fd));
    temp_file_unlinker.reset(new ScopedPathUnlinker(temp_file_path));
    TEST_AND_RETURN_FALSE(data_file_fd >= 0);
    ScopedFdCloser data_file_fd_closer(&data_file_fd);

    // Generate the operations using the strategy we selected above.
    TEST_AND_RETURN_FALSE(strategy->GenerateOperations(config,
                                                       data_file_fd,
                                                       &data_file_size,
                                                       &rootfs_ops,
                                                       &kernel_ops));
  }

  if (!config.source.ImageInfoIsEmpty())
    *(manifest.mutable_old_image_info()) = config.source.image_info;

  if (!config.target.ImageInfoIsEmpty())
    *(manifest.mutable_new_image_info()) = config.target.image_info;

  // Filter the no-operations. OperationsGenerators should not output this kind
  // of operations normally, but this is an extra step to fix that if
  // happened.
  DeltaDiffGenerator::FilterNoopOperations(&rootfs_ops);
  DeltaDiffGenerator::FilterNoopOperations(&kernel_ops);

  OperationNameMap op_name_map;
  InstallOperationsToManifest(rootfs_ops, kernel_ops, &manifest, &op_name_map);
  manifest.set_block_size(config.block_size);

  // Reorder the data blobs with the newly ordered manifest.
  string ordered_blobs_path;
  TEST_AND_RETURN_FALSE(utils::MakeTempFile(
      "CrAU_temp_data.ordered.XXXXXX",
      &ordered_blobs_path,
      nullptr));
  ScopedPathUnlinker ordered_blobs_unlinker(ordered_blobs_path);
  TEST_AND_RETURN_FALSE(
      DeltaDiffGenerator::ReorderDataBlobs(&manifest,
                                           temp_file_path,
                                           ordered_blobs_path));
  temp_file_unlinker.reset();

  // Check that install op blobs are in order.
  uint64_t next_blob_offset = 0;
  {
    for (int i = 0; i < (manifest.install_operations_size() +
                         manifest.kernel_install_operations_size()); i++) {
      DeltaArchiveManifest_InstallOperation* op =
          i < manifest.install_operations_size() ?
          manifest.mutable_install_operations(i) :
          manifest.mutable_kernel_install_operations(
              i - manifest.install_operations_size());
      if (op->has_data_offset()) {
        if (op->data_offset() != next_blob_offset) {
          LOG(FATAL) << "bad blob offset! " << op->data_offset() << " != "
                     << next_blob_offset;
        }
        next_blob_offset += op->data_length();
      }
    }
  }

  // Signatures appear at the end of the blobs. Note the offset in the
  // manifest
  if (!private_key_path.empty()) {
    uint64_t signature_blob_length = 0;
    TEST_AND_RETURN_FALSE(
        PayloadSigner::SignatureBlobLength(vector<string>(1, private_key_path),
                                           &signature_blob_length));
    DeltaDiffGenerator::AddSignatureOp(
        next_blob_offset, signature_blob_length, &manifest);
  }

  TEST_AND_RETURN_FALSE(InitializePartitionInfos(config, &manifest));

  // Serialize protobuf
  string serialized_manifest;

  TEST_AND_RETURN_FALSE(manifest.AppendToString(&serialized_manifest));

  LOG(INFO) << "Writing final delta file header...";
  DirectFileWriter writer;
  TEST_AND_RETURN_FALSE_ERRNO(writer.Open(output_path.c_str(),
                                          O_WRONLY | O_CREAT | O_TRUNC,
                                          0644) == 0);
  ScopedFileWriterCloser writer_closer(&writer);

  // Write header
  TEST_AND_RETURN_FALSE(writer.Write(kDeltaMagic, strlen(kDeltaMagic)));

  // Write major version number
  TEST_AND_RETURN_FALSE(WriteUint64AsBigEndian(&writer, kMajorVersionNumber));

  // Write protobuf length
  TEST_AND_RETURN_FALSE(WriteUint64AsBigEndian(&writer,
                                               serialized_manifest.size()));

  // Write protobuf
  LOG(INFO) << "Writing final delta file protobuf... "
            << serialized_manifest.size();
  TEST_AND_RETURN_FALSE(writer.Write(serialized_manifest.data(),
                                     serialized_manifest.size()));

  // Append the data blobs
  LOG(INFO) << "Writing final delta file data blobs...";
  int blobs_fd = open(ordered_blobs_path.c_str(), O_RDONLY, 0);
  ScopedFdCloser blobs_fd_closer(&blobs_fd);
  TEST_AND_RETURN_FALSE(blobs_fd >= 0);
  for (;;) {
    vector<char> buf(config.block_size);
    ssize_t rc = read(blobs_fd, buf.data(), buf.size());
    if (0 == rc) {
      // EOF
      break;
    }
    TEST_AND_RETURN_FALSE_ERRNO(rc > 0);
    TEST_AND_RETURN_FALSE(writer.Write(buf.data(), rc));
  }

  // Write signature blob.
  if (!private_key_path.empty()) {
    LOG(INFO) << "Signing the update...";
    chromeos::Blob signature_blob;
    TEST_AND_RETURN_FALSE(PayloadSigner::SignPayload(
        output_path,
        vector<string>(1, private_key_path),
        &signature_blob));
    TEST_AND_RETURN_FALSE(writer.Write(signature_blob.data(),
                                       signature_blob.size()));
  }

  *metadata_size =
      strlen(kDeltaMagic) + 2 * sizeof(uint64_t) + serialized_manifest.size();
  ReportPayloadUsage(manifest, *metadata_size, op_name_map);

  LOG(INFO) << "All done. Successfully created delta file with "
            << "metadata size = " << *metadata_size;
  return true;
}

// Runs the bsdiff tool on two files and returns the resulting delta in
// 'out'. Returns true on success.
bool DeltaDiffGenerator::BsdiffFiles(const string& old_file,
                                     const string& new_file,
                                     chromeos::Blob* out) {
  const string kPatchFile = "delta.patchXXXXXX";
  string patch_file_path;

  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile(kPatchFile, &patch_file_path, nullptr));

  vector<string> cmd;
  cmd.push_back(kBsdiffPath);
  cmd.push_back(old_file);
  cmd.push_back(new_file);
  cmd.push_back(patch_file_path);

  int rc = 1;
  chromeos::Blob patch_file;
  TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &rc, nullptr));
  TEST_AND_RETURN_FALSE(rc == 0);
  TEST_AND_RETURN_FALSE(utils::ReadFile(patch_file_path, out));
  unlink(patch_file_path.c_str());
  return true;
}

void DeltaDiffGenerator::AddSignatureOp(uint64_t signature_blob_offset,
                                        uint64_t signature_blob_length,
                                        DeltaArchiveManifest* manifest) {
  LOG(INFO) << "Making room for signature in file";
  manifest->set_signatures_offset(signature_blob_offset);
  LOG(INFO) << "set? " << manifest->has_signatures_offset();
  // Add a dummy op at the end to appease older clients
  DeltaArchiveManifest_InstallOperation* dummy_op =
      manifest->add_kernel_install_operations();
  dummy_op->set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE);
  dummy_op->set_data_offset(signature_blob_offset);
  manifest->set_signatures_offset(signature_blob_offset);
  dummy_op->set_data_length(signature_blob_length);
  manifest->set_signatures_size(signature_blob_length);
  Extent* dummy_extent = dummy_op->add_dst_extents();
  // Tell the dummy op to write this data to a big sparse hole
  dummy_extent->set_start_block(kSparseHole);
  dummy_extent->set_num_blocks((signature_blob_length + kBlockSize - 1) /
                               kBlockSize);
}

void DeltaDiffGenerator::ClearSparseHoles(vector<Extent>* extents) {
  extents->erase(std::remove_if(extents->begin(), extents->end(), IsSparseHole),
                 extents->end());
}

};  // namespace chromeos_update_engine
