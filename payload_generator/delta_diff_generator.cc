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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>

#include "update_engine/delta_performer.h"
#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/ab_generator.h"
#include "update_engine/payload_generator/blob_file_writer.h"
#include "update_engine/payload_generator/delta_diff_utils.h"
#include "update_engine/payload_generator/full_update_generator.h"
#include "update_engine/payload_generator/inplace_generator.h"
#include "update_engine/payload_generator/payload_file.h"
#include "update_engine/utils.h"

using std::string;
using std::unique_ptr;
using std::vector;

namespace chromeos_update_engine {

// bytes
const size_t kRootFSPartitionSize = static_cast<size_t>(2) * 1024 * 1024 * 1024;
const size_t kBlockSize = 4096;  // bytes

bool GenerateUpdatePayloadFile(
    const PayloadGenerationConfig& config,
    const string& output_path,
    const string& private_key_path,
    uint64_t* metadata_size) {
  if (config.is_delta) {
    LOG_IF(WARNING, config.source.rootfs.size != config.target.rootfs.size)
        << "Old and new images have different block counts.";
    // TODO(deymo): Our tools only support growing the filesystem size during
    // an update. Remove this check when that's fixed. crbug.com/192136
    LOG_IF(FATAL, config.source.rootfs.size > config.target.rootfs.size)
        << "Shirking the rootfs size is not supported at the moment.";
  }

  // Sanity checks for the partition size.
  LOG(INFO) << "Rootfs partition size: " << config.rootfs_partition_size;
  LOG(INFO) << "Actual filesystem size: " << config.target.rootfs.size;

  LOG(INFO) << "Block count: "
            << config.target.rootfs.size / config.block_size;

  LOG_IF(INFO, config.source.kernel.path.empty())
      << "Will generate full kernel update.";

  const string kTempFileTemplate("CrAU_temp_data.XXXXXX");
  string temp_file_path;
  off_t data_file_size = 0;

  LOG(INFO) << "Reading files...";

  // Create empty payload file object.
  PayloadFile payload;
  TEST_AND_RETURN_FALSE(payload.Init(config));

  vector<AnnotatedOperation> rootfs_ops;
  vector<AnnotatedOperation> kernel_ops;

  // Select payload generation strategy based on the config.
  unique_ptr<OperationsGenerator> strategy;
  if (config.is_delta) {
    // We don't efficiently support deltas on squashfs. For now, we will
    // produce full operations in that case.
    if (utils::IsSquashfsFilesystem(config.target.rootfs.path)) {
      LOG(INFO) << "Using generator FullUpdateGenerator() for squashfs deltas";
      strategy.reset(new FullUpdateGenerator());
    } else if (utils::IsExtFilesystem(config.target.rootfs.path)) {
      // Delta update (with possibly a full kernel update).
      if (config.minor_version == kInPlaceMinorPayloadVersion) {
        LOG(INFO) << "Using generator InplaceGenerator()";
        strategy.reset(new InplaceGenerator());
      } else if (config.minor_version == kSourceMinorPayloadVersion) {
        LOG(INFO) << "Using generator ABGenerator()";
        strategy.reset(new ABGenerator());
      } else {
        LOG(ERROR) << "Unsupported minor version given for delta payload: "
                   << config.minor_version;
        return false;
      }
    } else {
      LOG(ERROR) << "Unsupported filesystem for delta payload in "
                 << config.target.rootfs.path;
      return false;
    }
  } else {
    // Full update.
    LOG(INFO) << "Using generator FullUpdateGenerator()";
    strategy.reset(new FullUpdateGenerator());
  }

  int data_file_fd;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile(kTempFileTemplate, &temp_file_path, &data_file_fd));
  unique_ptr<ScopedPathUnlinker> temp_file_unlinker(
      new ScopedPathUnlinker(temp_file_path));
  TEST_AND_RETURN_FALSE(data_file_fd >= 0);

  {
    ScopedFdCloser data_file_fd_closer(&data_file_fd);
    BlobFileWriter blob_file(data_file_fd, &data_file_size);
    // Generate the operations using the strategy we selected above.
    TEST_AND_RETURN_FALSE(strategy->GenerateOperations(config,
                                                       &blob_file,
                                                       &rootfs_ops,
                                                       &kernel_ops));
  }

  // Filter the no-operations. OperationsGenerators should not output this kind
  // of operations normally, but this is an extra step to fix that if
  // happened.
  diff_utils::FilterNoopOperations(&rootfs_ops);
  diff_utils::FilterNoopOperations(&kernel_ops);

  // Write payload file to disk.
  payload.AddPartitionOperations(PartitionName::kRootfs, rootfs_ops);
  payload.AddPartitionOperations(PartitionName::kKernel, kernel_ops);
  payload.WritePayload(output_path, temp_file_path, private_key_path,
                       metadata_size);
  temp_file_unlinker.reset();

  LOG(INFO) << "All done. Successfully created delta file with "
            << "metadata size = " << *metadata_size;
  return true;
}

};  // namespace chromeos_update_engine
