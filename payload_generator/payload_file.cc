// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/payload_file.h"

#include <algorithm>

#include "update_engine/file_writer.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/annotated_operation.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/delta_diff_utils.h"
#include "update_engine/payload_generator/payload_signer.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

const uint64_t kMajorVersionNumber = 1;

static const char* kInstallOperationTypes[] = {
  "REPLACE",
  "REPLACE_BZ",
  "MOVE",
  "BSDIFF",
  "SOURCE_COPY",
  "SOURCE_BSDIFF"
};

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

// Writes the uint64_t passed in in host-endian to the file as big-endian.
// Returns true on success.
bool WriteUint64AsBigEndian(FileWriter* writer, const uint64_t value) {
  uint64_t value_be = htobe64(value);
  TEST_AND_RETURN_FALSE(writer->Write(&value_be, sizeof(value_be)));
  return true;
}

}  // namespace

const vector<PartitionName> PayloadFile::partition_disk_order_ = {
  PartitionName::kRootfs,
  PartitionName::kKernel,
};

bool PayloadFile::Init(const PayloadGenerationConfig& config) {
  manifest_.set_minor_version(config.minor_version);

  if (!config.source.ImageInfoIsEmpty())
    *(manifest_.mutable_old_image_info()) = config.source.image_info;

  if (!config.target.ImageInfoIsEmpty())
    *(manifest_.mutable_new_image_info()) = config.target.image_info;

  manifest_.set_block_size(config.block_size);

  // Initialize the PartitionInfo objects if present.
  if (!config.source.kernel.path.empty()) {
    TEST_AND_RETURN_FALSE(diff_utils::InitializePartitionInfo(
        config.source.kernel,
        manifest_.mutable_old_kernel_info()));
  }
  TEST_AND_RETURN_FALSE(diff_utils::InitializePartitionInfo(
      config.target.kernel,
      manifest_.mutable_new_kernel_info()));
  if (!config.source.rootfs.path.empty()) {
    TEST_AND_RETURN_FALSE(diff_utils::InitializePartitionInfo(
        config.source.rootfs,
        manifest_.mutable_old_rootfs_info()));
  }
  TEST_AND_RETURN_FALSE(diff_utils::InitializePartitionInfo(
      config.target.rootfs,
      manifest_.mutable_new_rootfs_info()));
  return true;
}

void PayloadFile::AddPartitionOperations(
    PartitionName name,
    const vector<AnnotatedOperation>& aops) {
  aops_map_[name].insert(aops_map_[name].end(), aops.begin(), aops.end());
}

bool PayloadFile::WritePayload(const string& payload_file,
                               const string& data_blobs_path,
                               const string& private_key_path,
                               uint64_t* medatata_size_out) {
  // Reorder the data blobs with the manifest_.
  string ordered_blobs_path;
  TEST_AND_RETURN_FALSE(utils::MakeTempFile(
      "CrAU_temp_data.ordered.XXXXXX",
      &ordered_blobs_path,
      nullptr));
  ScopedPathUnlinker ordered_blobs_unlinker(ordered_blobs_path);
  TEST_AND_RETURN_FALSE(ReorderDataBlobs(data_blobs_path, ordered_blobs_path));

  // Copy the operations from the aops_map_ to the manifest.
  manifest_.clear_install_operations();
  manifest_.clear_kernel_install_operations();
  for (PartitionName name : partition_disk_order_) {
    for (const AnnotatedOperation& aop : aops_map_[name]) {
      if (name == PartitionName::kKernel) {
        *manifest_.add_kernel_install_operations() = aop.op;
      } else {
        *manifest_.add_install_operations() = aop.op;
      }
    }
  }

  // Check that install op blobs are in order.
  uint64_t next_blob_offset = 0;
  {
    for (int i = 0; i < (manifest_.install_operations_size() +
                         manifest_.kernel_install_operations_size()); i++) {
      InstallOperation* op = i < manifest_.install_operations_size()
                                 ? manifest_.mutable_install_operations(i)
                                 : manifest_.mutable_kernel_install_operations(
                                       i - manifest_.install_operations_size());
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
  // manifest_.
  if (!private_key_path.empty()) {
    uint64_t signature_blob_length = 0;
    TEST_AND_RETURN_FALSE(
        PayloadSigner::SignatureBlobLength(vector<string>(1, private_key_path),
                                           &signature_blob_length));
    AddSignatureOp(next_blob_offset, signature_blob_length, &manifest_);
  }

    // Serialize protobuf
  string serialized_manifest;
  TEST_AND_RETURN_FALSE(manifest_.AppendToString(&serialized_manifest));

  LOG(INFO) << "Writing final delta file header...";
  DirectFileWriter writer;
  TEST_AND_RETURN_FALSE_ERRNO(writer.Open(payload_file.c_str(),
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
    vector<char> buf(1024 * 1024);
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
        payload_file,
        vector<string>(1, private_key_path),
        &signature_blob));
    TEST_AND_RETURN_FALSE(writer.Write(signature_blob.data(),
                                       signature_blob.size()));
  }

  *medatata_size_out =
      strlen(kDeltaMagic) + 2 * sizeof(uint64_t) + serialized_manifest.size();
  ReportPayloadUsage(*medatata_size_out);
  return true;
}

bool PayloadFile::ReorderDataBlobs(
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

  for (PartitionName name : partition_disk_order_) {
    for (AnnotatedOperation& aop : aops_map_[name]) {
      if (!aop.op.has_data_offset())
        continue;
      CHECK(aop.op.has_data_length());
      chromeos::Blob buf(aop.op.data_length());
      ssize_t rc = pread(in_fd, buf.data(), buf.size(), aop.op.data_offset());
      TEST_AND_RETURN_FALSE(rc == static_cast<ssize_t>(buf.size()));

      // Add the hash of the data blobs for this operation
      TEST_AND_RETURN_FALSE(AddOperationHash(&aop.op, buf));

      aop.op.set_data_offset(out_file_size);
      TEST_AND_RETURN_FALSE(writer.Write(buf.data(), buf.size()));
      out_file_size += buf.size();
    }
  }
  return true;
}

bool PayloadFile::AddOperationHash(InstallOperation* op,
                                   const chromeos::Blob& buf) {
  OmahaHashCalculator hasher;
  TEST_AND_RETURN_FALSE(hasher.Update(buf.data(), buf.size()));
  TEST_AND_RETURN_FALSE(hasher.Finalize());
  const chromeos::Blob& hash = hasher.raw_hash();
  op->set_data_sha256_hash(hash.data(), hash.size());
  return true;
}

void PayloadFile::ReportPayloadUsage(uint64_t metadata_size) const {
  vector<DeltaObject> objects;
  off_t total_size = 0;

  for (PartitionName name : partition_disk_order_) {
    const auto& partition_aops = aops_map_.find(name);
    if (partition_aops == aops_map_.end())
      continue;
    for (const AnnotatedOperation& aop : partition_aops->second) {
      objects.push_back(DeltaObject(aop.name,
                                    aop.op.type(),
                                    aop.op.data_length()));
      total_size += aop.op.data_length();
    }
  }

  objects.push_back(DeltaObject("<manifest-metadata>",
                                -1,
                                metadata_size));
  total_size += metadata_size;

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

void AddSignatureOp(uint64_t signature_blob_offset,
                    uint64_t signature_blob_length,
                    DeltaArchiveManifest* manifest) {
  LOG(INFO) << "Making room for signature in file";
  manifest->set_signatures_offset(signature_blob_offset);
  LOG(INFO) << "set? " << manifest->has_signatures_offset();
  // Add a dummy op at the end to appease older clients
  InstallOperation* dummy_op = manifest->add_kernel_install_operations();
  dummy_op->set_type(InstallOperation::REPLACE);
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

}  // namespace chromeos_update_engine
