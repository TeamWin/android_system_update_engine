//
// Copyright (C) 2010 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <chromeos/flag_helper.h>

#include "update_engine/delta_performer.h"
#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/delta_diff_utils.h"
#include "update_engine/payload_generator/payload_generation_config.h"
#include "update_engine/payload_generator/payload_signer.h"
#include "update_engine/payload_verifier.h"
#include "update_engine/prefs.h"
#include "update_engine/terminator.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

// This file contains a simple program that takes an old path, a new path,
// and an output file as arguments and the path to an output file and
// generates a delta that can be sent to Chrome OS clients.

using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

void ParseSignatureSizes(const string& signature_sizes_flag,
                         vector<int>* signature_sizes) {
  signature_sizes->clear();
  vector<string> split_strings;

  base::SplitString(signature_sizes_flag, ':', &split_strings);
  for (const string& str : split_strings) {
    int size = 0;
    bool parsing_successful = base::StringToInt(str, &size);
    LOG_IF(FATAL, !parsing_successful)
        << "Invalid signature size: " << str;

    LOG_IF(FATAL, size != (2048 / 8)) <<
        "Only signature sizes of 256 bytes are supported.";

    signature_sizes->push_back(size);
  }
}

bool ParseImageInfo(const string& channel,
                    const string& board,
                    const string& version,
                    const string& key,
                    const string& build_channel,
                    const string& build_version,
                    ImageInfo* image_info) {
  // All of these arguments should be present or missing.
  bool empty = channel.empty();

  CHECK_EQ(channel.empty(), empty);
  CHECK_EQ(board.empty(), empty);
  CHECK_EQ(version.empty(), empty);
  CHECK_EQ(key.empty(), empty);

  if (empty)
    return false;

  image_info->set_channel(channel);
  image_info->set_board(board);
  image_info->set_version(version);
  image_info->set_key(key);

  image_info->set_build_channel(
      build_channel.empty() ? channel : build_channel);

  image_info->set_build_version(
      build_version.empty() ? version : build_version);

  return true;
}

void CalculatePayloadHashForSigning(const vector<int> &sizes,
                                    const string& out_hash_file,
                                    const string& in_file) {
  LOG(INFO) << "Calculating payload hash for signing.";
  LOG_IF(FATAL, in_file.empty())
      << "Must pass --in_file to calculate hash for signing.";
  LOG_IF(FATAL, out_hash_file.empty())
      << "Must pass --out_hash_file to calculate hash for signing.";

  chromeos::Blob hash;
  bool result = PayloadSigner::HashPayloadForSigning(in_file, sizes,
                                                     &hash);
  CHECK(result);

  result = utils::WriteFile(out_hash_file.c_str(), hash.data(), hash.size());
  CHECK(result);
  LOG(INFO) << "Done calculating payload hash for signing.";
}


void CalculateMetadataHashForSigning(const vector<int> &sizes,
                                     const string& out_metadata_hash_file,
                                     const string& in_file) {
  LOG(INFO) << "Calculating metadata hash for signing.";
  LOG_IF(FATAL, in_file.empty())
      << "Must pass --in_file to calculate metadata hash for signing.";
  LOG_IF(FATAL, out_metadata_hash_file.empty())
      << "Must pass --out_metadata_hash_file to calculate metadata hash.";

  chromeos::Blob hash;
  bool result = PayloadSigner::HashMetadataForSigning(in_file, sizes,
                                                      &hash);
  CHECK(result);

  result = utils::WriteFile(out_metadata_hash_file.c_str(), hash.data(),
                            hash.size());
  CHECK(result);

  LOG(INFO) << "Done calculating metadata hash for signing.";
}

void SignPayload(const string& in_file,
                 const string& out_file,
                 const string& signature_file) {
  LOG(INFO) << "Signing payload.";
  LOG_IF(FATAL, in_file.empty())
      << "Must pass --in_file to sign payload.";
  LOG_IF(FATAL, out_file.empty())
      << "Must pass --out_file to sign payload.";
  LOG_IF(FATAL, signature_file.empty())
      << "Must pass --signature_file to sign payload.";
  vector<chromeos::Blob> signatures;
  vector<string> signature_files;
  base::SplitString(signature_file, ':', &signature_files);
  for (const string& signature_file : signature_files) {
    chromeos::Blob signature;
    CHECK(utils::ReadFile(signature_file, &signature));
    signatures.push_back(signature);
  }
  uint64_t final_metadata_size;
  CHECK(PayloadSigner::AddSignatureToPayload(
      in_file, signatures, out_file, &final_metadata_size));
  LOG(INFO) << "Done signing payload. Final metadata size = "
            << final_metadata_size;
}

void VerifySignedPayload(const string& in_file,
                         const string& public_key,
                         int public_key_version) {
  LOG(INFO) << "Verifying signed payload.";
  LOG_IF(FATAL, in_file.empty())
      << "Must pass --in_file to verify signed payload.";
  LOG_IF(FATAL, public_key.empty())
      << "Must pass --public_key to verify signed payload.";
  CHECK(PayloadVerifier::VerifySignedPayload(in_file, public_key,
                                             public_key_version));
  LOG(INFO) << "Done verifying signed payload.";
}

void ApplyDelta(const string& in_file,
                const string& old_kernel,
                const string& old_rootfs,
                const string& prefs_dir) {
  LOG(INFO) << "Applying delta.";
  LOG_IF(FATAL, old_rootfs.empty())
      << "Must pass --old_image to apply delta.";
  Prefs prefs;
  InstallPlan install_plan;
  LOG(INFO) << "Setting up preferences under: " << prefs_dir;
  LOG_IF(ERROR, !prefs.Init(base::FilePath(prefs_dir)))
      << "Failed to initialize preferences.";
  // Get original checksums
  LOG(INFO) << "Calculating original checksums";
  PartitionInfo kern_info, root_info;
  ImageConfig old_image;
  old_image.kernel.path = old_kernel;
  old_image.rootfs.path = old_rootfs;
  CHECK(old_image.LoadImageSize());
  CHECK(diff_utils::InitializePartitionInfo(old_image.kernel, &kern_info));
  CHECK(diff_utils::InitializePartitionInfo(old_image.rootfs, &root_info));
  install_plan.kernel_hash.assign(kern_info.hash().begin(),
                                  kern_info.hash().end());
  install_plan.rootfs_hash.assign(root_info.hash().begin(),
                                  root_info.hash().end());
  DeltaPerformer performer(&prefs, nullptr, &install_plan);
  CHECK_EQ(performer.Open(old_rootfs.c_str(), 0, 0), 0);
  CHECK(performer.OpenKernel(old_kernel.c_str()));
  chromeos::Blob buf(1024 * 1024);
  int fd = open(in_file.c_str(), O_RDONLY, 0);
  CHECK_GE(fd, 0);
  ScopedFdCloser fd_closer(&fd);
  for (off_t offset = 0;; offset += buf.size()) {
    ssize_t bytes_read;
    CHECK(utils::PReadAll(fd, buf.data(), buf.size(), offset, &bytes_read));
    if (bytes_read == 0)
      break;
    CHECK_EQ(performer.Write(buf.data(), bytes_read), bytes_read);
  }
  CHECK_EQ(performer.Close(), 0);
  DeltaPerformer::ResetUpdateProgress(&prefs, false);
  LOG(INFO) << "Done applying delta.";
}

int Main(int argc, char** argv) {
  DEFINE_string(old_image, "", "Path to the old rootfs");
  DEFINE_string(new_image, "", "Path to the new rootfs");
  DEFINE_string(old_kernel, "", "Path to the old kernel partition image");
  DEFINE_string(new_kernel, "", "Path to the new kernel partition image");
  DEFINE_string(in_file, "",
                "Path to input delta payload file used to hash/sign payloads "
                "and apply delta over old_image (for debugging)");
  DEFINE_string(out_file, "", "Path to output delta payload file");
  DEFINE_string(out_hash_file, "", "Path to output hash file");
  DEFINE_string(out_metadata_hash_file, "",
                "Path to output metadata hash file");
  DEFINE_string(private_key, "", "Path to private key in .pem format");
  DEFINE_string(public_key, "", "Path to public key in .pem format");
  DEFINE_int32(public_key_version,
               chromeos_update_engine::kSignatureMessageCurrentVersion,
               "Key-check version # of client");
  DEFINE_string(prefs_dir, "/tmp/update_engine_prefs",
                "Preferences directory, used with apply_delta");
  DEFINE_string(signature_size, "",
                "Raw signature size used for hash calculation. "
                "You may pass in multiple sizes by colon separating them. E.g. "
                "2048:2048:4096 will assume 3 signatures, the first two with "
                "2048 size and the last 4096.");
  DEFINE_string(signature_file, "",
                "Raw signature file to sign payload with. To pass multiple "
                "signatures, use a single argument with a colon between paths, "
                "e.g. /path/to/sig:/path/to/next:/path/to/last_sig . Each "
                "signature will be assigned a client version, starting from "
                "kSignatureOriginalVersion.");
  DEFINE_string(metadata_signature_file, "",
                "Raw signature file with the signature of the metadata hash. "
                "To pass multiple signatures, use a single argument with a "
                "colon between paths, "
                "e.g. /path/to/sig:/path/to/next:/path/to/last_sig .");
  DEFINE_int32(chunk_size, 200 * 1024 * 1024,
               "Payload chunk size (-1 for whole files)");
  DEFINE_uint64(rootfs_partition_size,
               chromeos_update_engine::kRootFSPartitionSize,
               "RootFS partition size for the image once installed");
  DEFINE_uint64(major_version, 1,
               "The major version of the payload being generated.");
  DEFINE_int32(minor_version, -1,
               "The minor version of the payload being generated "
               "(-1 means autodetect).");

  DEFINE_string(old_channel, "",
                "The channel for the old image. 'dev-channel', 'npo-channel', "
                "etc. Ignored, except during delta generation.");
  DEFINE_string(old_board, "",
                "The board for the old image. 'x86-mario', 'lumpy', "
                "etc. Ignored, except during delta generation.");
  DEFINE_string(old_version, "",
                "The build version of the old image. 1.2.3, etc.");
  DEFINE_string(old_key, "",
                "The key used to sign the old image. 'premp', 'mp', 'mp-v3',"
                " etc");
  DEFINE_string(old_build_channel, "",
                "The channel for the build of the old image. 'dev-channel', "
                "etc, but will never contain special channels such as "
                "'npo-channel'. Ignored, except during delta generation.");
  DEFINE_string(old_build_version, "",
                "The version of the build containing the old image.");

  DEFINE_string(new_channel, "",
                "The channel for the new image. 'dev-channel', 'npo-channel', "
                "etc. Ignored, except during delta generation.");
  DEFINE_string(new_board, "",
                "The board for the new image. 'x86-mario', 'lumpy', "
                "etc. Ignored, except during delta generation.");
  DEFINE_string(new_version, "",
                "The build version of the new image. 1.2.3, etc.");
  DEFINE_string(new_key, "",
                "The key used to sign the new image. 'premp', 'mp', 'mp-v3',"
                " etc");
  DEFINE_string(new_build_channel, "",
                "The channel for the build of the new image. 'dev-channel', "
                "etc, but will never contain special channels such as "
                "'npo-channel'. Ignored, except during delta generation.");
  DEFINE_string(new_build_version, "",
                "The version of the build containing the new image.");

  chromeos::FlagHelper::Init(argc, argv,
      "Generates a payload to provide to ChromeOS' update_engine.\n\n"
      "This tool can create full payloads and also delta payloads if the src\n"
      "image is provided. It also provides debugging options to apply, sign\n"
      "and verify payloads.");
  Terminator::Init();

  logging::LoggingSettings log_settings;
  log_settings.log_file     = "delta_generator.log";
  log_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  log_settings.lock_log     = logging::LOCK_LOG_FILE;
  log_settings.delete_old   = logging::APPEND_TO_OLD_LOG_FILE;

  logging::InitLogging(log_settings);

  vector<int> signature_sizes;
  ParseSignatureSizes(FLAGS_signature_size, &signature_sizes);

  if (!FLAGS_out_hash_file.empty() || !FLAGS_out_metadata_hash_file.empty()) {
    if (!FLAGS_out_hash_file.empty()) {
      CalculatePayloadHashForSigning(signature_sizes, FLAGS_out_hash_file,
                                     FLAGS_in_file);
    }
    if (!FLAGS_out_metadata_hash_file.empty()) {
      CalculateMetadataHashForSigning(signature_sizes,
                                      FLAGS_out_metadata_hash_file,
                                      FLAGS_in_file);
    }
    return 0;
  }
  if (!FLAGS_signature_file.empty()) {
    SignPayload(FLAGS_in_file, FLAGS_out_file, FLAGS_signature_file);
    return 0;
  }
  if (!FLAGS_public_key.empty()) {
    VerifySignedPayload(FLAGS_in_file, FLAGS_public_key,
                        FLAGS_public_key_version);
    return 0;
  }
  if (!FLAGS_in_file.empty()) {
    ApplyDelta(FLAGS_in_file, FLAGS_old_kernel, FLAGS_old_image,
               FLAGS_prefs_dir);
    return 0;
  }

  // A payload generation was requested. Convert the flags to a
  // PayloadGenerationConfig.
  PayloadGenerationConfig payload_config;
  payload_config.source.rootfs.path = FLAGS_old_image;
  payload_config.source.kernel.path = FLAGS_old_kernel;

  payload_config.target.rootfs.path = FLAGS_new_image;
  payload_config.target.kernel.path = FLAGS_new_kernel;

  // Use the default soft_chunk_size defined in the config.
  payload_config.hard_chunk_size = FLAGS_chunk_size;
  payload_config.block_size = kBlockSize;

  // The kernel and rootfs size is never passed to the delta_generator, so we
  // need to detect those from the provided files.
  if (!FLAGS_old_image.empty()) {
    CHECK(payload_config.source.LoadImageSize());
  }
  if (!FLAGS_new_image.empty()) {
    CHECK(payload_config.target.LoadImageSize());
  }

  payload_config.is_delta = !FLAGS_old_image.empty();

  CHECK(!FLAGS_out_file.empty());

  // Ignore failures. These are optional arguments.
  ParseImageInfo(FLAGS_new_channel,
                 FLAGS_new_board,
                 FLAGS_new_version,
                 FLAGS_new_key,
                 FLAGS_new_build_channel,
                 FLAGS_new_build_version,
                 &payload_config.target.image_info);

  // Ignore failures. These are optional arguments.
  ParseImageInfo(FLAGS_old_channel,
                 FLAGS_old_board,
                 FLAGS_old_version,
                 FLAGS_old_key,
                 FLAGS_old_build_channel,
                 FLAGS_old_build_version,
                 &payload_config.source.image_info);

  payload_config.rootfs_partition_size = FLAGS_rootfs_partition_size;

  if (payload_config.is_delta) {
    // Avoid opening the filesystem interface for full payloads.
    CHECK(payload_config.target.rootfs.OpenFilesystem());
    CHECK(payload_config.target.kernel.OpenFilesystem());
    CHECK(payload_config.source.rootfs.OpenFilesystem());
    CHECK(payload_config.source.kernel.OpenFilesystem());
  }

  payload_config.major_version = FLAGS_major_version;
  LOG(INFO) << "Using provided major_version=" << FLAGS_major_version;

  if (FLAGS_minor_version == -1) {
    // Autodetect minor_version by looking at the update_engine.conf in the old
    // image.
    if (payload_config.is_delta) {
      CHECK(payload_config.source.rootfs.fs_interface);
      chromeos::KeyValueStore store;
      uint32_t minor_version;
      if (payload_config.source.rootfs.fs_interface->LoadSettings(&store) &&
          utils::GetMinorVersion(store, &minor_version)) {
        payload_config.minor_version = minor_version;
      } else {
        payload_config.minor_version = kInPlaceMinorPayloadVersion;
      }
    } else {
      payload_config.minor_version = kFullPayloadMinorVersion;
    }
    LOG(INFO) << "Auto-detected minor_version=" << payload_config.minor_version;
  } else {
    payload_config.minor_version = FLAGS_minor_version;
    LOG(INFO) << "Using provided minor_version=" << FLAGS_minor_version;
  }

  if (payload_config.is_delta) {
    LOG(INFO) << "Generating delta update";
  } else {
    LOG(INFO) << "Generating full update";
  }

  // From this point, all the options have been parsed.
  if (!payload_config.Validate()) {
    LOG(ERROR) << "Invalid options passed. See errors above.";
    return 1;
  }

  uint64_t metadata_size;
  if (!GenerateUpdatePayloadFile(payload_config,
                                 FLAGS_out_file,
                                 FLAGS_private_key,
                                 &metadata_size)) {
    return 1;
  }

  return 0;
}

}  // namespace

}  // namespace chromeos_update_engine

int main(int argc, char** argv) {
  return chromeos_update_engine::Main(argc, argv);
}
