//
// Copyright (C) 2011 The Android Open Source Project
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

#include "update_engine/payload_generator/payload_signer.h"

#include <endian.h>

#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <brillo/data_encoding.h>
#include <openssl/pem.h>

#include "update_engine/delta_performer.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/delta_diff_generator.h"
#include "update_engine/payload_generator/payload_file.h"
#include "update_engine/payload_verifier.h"
#include "update_engine/subprocess.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

// The payload verifier will check all the signatures included in the payload
// regardless of the version field. Old version of the verifier require the
// version field to be included and be 1.
const uint32_t kSignatureMessageLegacyVersion = 1;

// Given raw |signatures|, packs them into a protobuf and serializes it into a
// binary blob. Returns true on success, false otherwise.
bool ConvertSignatureToProtobufBlob(const vector<brillo::Blob>& signatures,
                                    brillo::Blob* out_signature_blob) {
  // Pack it into a protobuf
  Signatures out_message;
  for (const brillo::Blob& signature : signatures) {
    Signatures_Signature* sig_message = out_message.add_signatures();
    // Set all the signatures with the same version number.
    sig_message->set_version(kSignatureMessageLegacyVersion);
    sig_message->set_data(signature.data(), signature.size());
  }

  // Serialize protobuf
  string serialized;
  TEST_AND_RETURN_FALSE(out_message.AppendToString(&serialized));
  out_signature_blob->insert(out_signature_blob->end(),
                             serialized.begin(),
                             serialized.end());
  LOG(INFO) << "Signature blob size: " << out_signature_blob->size();
  return true;
}

// Given an unsigned payload under |payload_path| and the |signature_blob_size|
// generates an updated payload that includes a dummy signature op in its
// manifest. It populates |out_metadata_size| with the size of the final
// manifest after adding the dummy signature operation, and
// |out_signatures_offset| with the expected offset for the new blob. Returns
// true on success, false otherwise.
bool AddSignatureOpToPayload(const string& payload_path,
                             uint64_t signature_blob_size,
                             brillo::Blob* out_payload,
                             uint64_t* out_metadata_size,
                             uint64_t* out_signatures_offset) {
  const int kProtobufOffset = 20;
  const int kProtobufSizeOffset = 12;

  // Loads the payload.
  brillo::Blob payload;
  DeltaArchiveManifest manifest;
  uint64_t metadata_size, major_version;
  uint32_t metadata_signature_size;
  TEST_AND_RETURN_FALSE(PayloadSigner::LoadPayload(payload_path, &payload,
      &manifest, &major_version, &metadata_size, &metadata_signature_size));

  // Is there already a signature op in place?
  if (manifest.has_signatures_size()) {
    // The signature op is tied to the size of the signature blob, but not it's
    // contents. We don't allow the manifest to change if there is already an op
    // present, because that might invalidate previously generated
    // hashes/signatures.
    if (manifest.signatures_size() != signature_blob_size) {
      LOG(ERROR) << "Attempt to insert different signature sized blob. "
                 << "(current:" << manifest.signatures_size()
                 << "new:" << signature_blob_size << ")";
      return false;
    }

    LOG(INFO) << "Matching signature sizes already present.";
  } else {
    // Updates the manifest to include the signature operation.
    PayloadSigner::AddSignatureOp(payload.size() - metadata_size,
                                  signature_blob_size,
                                  &manifest);

    // Updates the payload to include the new manifest.
    string serialized_manifest;
    TEST_AND_RETURN_FALSE(manifest.AppendToString(&serialized_manifest));
    LOG(INFO) << "Updated protobuf size: " << serialized_manifest.size();
    payload.erase(payload.begin() + kProtobufOffset,
                  payload.begin() + metadata_size);
    payload.insert(payload.begin() + kProtobufOffset,
                   serialized_manifest.begin(),
                   serialized_manifest.end());

    // Updates the protobuf size.
    uint64_t size_be = htobe64(serialized_manifest.size());
    memcpy(&payload[kProtobufSizeOffset], &size_be, sizeof(size_be));
    metadata_size = serialized_manifest.size() + kProtobufOffset;

    LOG(INFO) << "Updated payload size: " << payload.size();
    LOG(INFO) << "Updated metadata size: " << metadata_size;
  }

  out_payload->swap(payload);
  *out_metadata_size = metadata_size;
  *out_signatures_offset = metadata_size + manifest.signatures_offset();
  LOG(INFO) << "Signature Blob Offset: " << *out_signatures_offset;
  return true;
}
}  // namespace

void PayloadSigner::AddSignatureOp(uint64_t signature_blob_offset,
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

bool PayloadSigner::LoadPayload(const std::string& payload_path,
                                brillo::Blob* out_payload,
                                DeltaArchiveManifest* out_manifest,
                                uint64_t* out_major_version,
                                uint64_t* out_metadata_size,
                                uint32_t* out_metadata_signature_size) {
  brillo::Blob payload;
  TEST_AND_RETURN_FALSE(utils::ReadFile(payload_path, &payload));
  TEST_AND_RETURN_FALSE(payload.size() >=
                        DeltaPerformer::kMaxPayloadHeaderSize);
  const uint8_t* read_pointer = payload.data();
  TEST_AND_RETURN_FALSE(
      memcmp(read_pointer, kDeltaMagic, sizeof(kDeltaMagic)) == 0);
  read_pointer += sizeof(kDeltaMagic);

  uint64_t major_version;
  memcpy(&major_version, read_pointer, sizeof(major_version));
  read_pointer += sizeof(major_version);
  major_version = be64toh(major_version);
  TEST_AND_RETURN_FALSE(major_version == kChromeOSMajorPayloadVersion ||
                        major_version == kBrilloMajorPayloadVersion);
  if (out_major_version)
    *out_major_version = major_version;

  uint64_t manifest_size = 0;
  memcpy(&manifest_size, read_pointer, sizeof(manifest_size));
  read_pointer += sizeof(manifest_size);
  manifest_size = be64toh(manifest_size);

  uint32_t metadata_signature_size = 0;
  if (major_version == kBrilloMajorPayloadVersion) {
    memcpy(&metadata_signature_size, read_pointer,
           sizeof(metadata_signature_size));
    read_pointer += sizeof(metadata_signature_size);
    metadata_signature_size = be32toh(metadata_signature_size);
  }
  if (out_metadata_signature_size)
    *out_metadata_signature_size = metadata_signature_size;

  *out_metadata_size = read_pointer - payload.data() + manifest_size;
  TEST_AND_RETURN_FALSE(payload.size() >= *out_metadata_size);
  if (out_manifest)
    TEST_AND_RETURN_FALSE(
        out_manifest->ParseFromArray(read_pointer, manifest_size));
  *out_payload = std::move(payload);
  return true;
}

bool PayloadSigner::VerifySignedPayload(const string& payload_path,
                                        const string& public_key_path) {
  brillo::Blob payload;
  DeltaArchiveManifest manifest;
  uint64_t metadata_size, major_version;
  uint32_t metadata_signature_size;
  TEST_AND_RETURN_FALSE(LoadPayload(payload_path, &payload, &manifest,
      &major_version, &metadata_size, &metadata_signature_size));
  TEST_AND_RETURN_FALSE(manifest.has_signatures_offset() &&
                        manifest.has_signatures_size());
  CHECK_EQ(payload.size(),
           metadata_size + manifest.signatures_offset() +
           manifest.signatures_size());
  brillo::Blob signature_blob(
      payload.begin() + metadata_size + manifest.signatures_offset(),
      payload.end());
  brillo::Blob hash;
  TEST_AND_RETURN_FALSE(OmahaHashCalculator::RawHashOfBytes(
      payload.data(), metadata_size + manifest.signatures_offset(), &hash));
  TEST_AND_RETURN_FALSE(PayloadVerifier::PadRSA2048SHA256Hash(&hash));
  TEST_AND_RETURN_FALSE(PayloadVerifier::VerifySignature(
      signature_blob, public_key_path, hash));
  return true;
}

bool PayloadSigner::SignHash(const brillo::Blob& hash,
                             const string& private_key_path,
                             brillo::Blob* out_signature) {
  LOG(INFO) << "Signing hash with private key: " << private_key_path;
  string sig_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile("signature.XXXXXX", &sig_path, nullptr));
  ScopedPathUnlinker sig_path_unlinker(sig_path);

  string hash_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile("hash.XXXXXX", &hash_path, nullptr));
  ScopedPathUnlinker hash_path_unlinker(hash_path);
  // We expect unpadded SHA256 hash coming in
  TEST_AND_RETURN_FALSE(hash.size() == 32);
  brillo::Blob padded_hash(hash);
  PayloadVerifier::PadRSA2048SHA256Hash(&padded_hash);
  TEST_AND_RETURN_FALSE(utils::WriteFile(hash_path.c_str(),
                                         padded_hash.data(),
                                         padded_hash.size()));

  // This runs on the server, so it's okay to copy out and call openssl
  // executable rather than properly use the library.
  vector<string> cmd = {"openssl", "rsautl", "-raw", "-sign", "-inkey",
                        private_key_path, "-in", hash_path, "-out", sig_path};
  int return_code = 0;
  TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &return_code,
                                                    nullptr));
  TEST_AND_RETURN_FALSE(return_code == 0);

  brillo::Blob signature;
  TEST_AND_RETURN_FALSE(utils::ReadFile(sig_path, &signature));
  out_signature->swap(signature);
  return true;
}

bool PayloadSigner::SignPayload(const string& unsigned_payload_path,
                                const vector<string>& private_key_paths,
                                brillo::Blob* out_signature_blob) {
  brillo::Blob hash_data;
  TEST_AND_RETURN_FALSE(OmahaHashCalculator::RawHashOfFile(
      unsigned_payload_path, -1, &hash_data) ==
                        utils::FileSize(unsigned_payload_path));

  vector<brillo::Blob> signatures;
  for (const string& path : private_key_paths) {
    brillo::Blob signature;
    TEST_AND_RETURN_FALSE(SignHash(hash_data, path, &signature));
    signatures.push_back(signature);
  }
  TEST_AND_RETURN_FALSE(ConvertSignatureToProtobufBlob(signatures,
                                                       out_signature_blob));
  return true;
}

bool PayloadSigner::SignatureBlobLength(const vector<string>& private_key_paths,
                                        uint64_t* out_length) {
  DCHECK(out_length);

  string x_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile("signed_data.XXXXXX", &x_path, nullptr));
  ScopedPathUnlinker x_path_unlinker(x_path);
  TEST_AND_RETURN_FALSE(utils::WriteFile(x_path.c_str(), "x", 1));

  brillo::Blob sig_blob;
  TEST_AND_RETURN_FALSE(PayloadSigner::SignPayload(x_path,
                                                   private_key_paths,
                                                   &sig_blob));
  *out_length = sig_blob.size();
  return true;
}

bool PayloadSigner::PrepPayloadForHashing(
        const string& payload_path,
        const vector<int>& signature_sizes,
        brillo::Blob* payload_out,
        uint64_t* metadata_size_out,
        uint64_t* signatures_offset_out) {
  // TODO(petkov): Reduce memory usage -- the payload is manipulated in memory.

  // Loads the payload and adds the signature op to it.
  vector<brillo::Blob> signatures;
  for (int signature_size : signature_sizes) {
    signatures.emplace_back(signature_size, 0);
  }
  brillo::Blob signature_blob;
  TEST_AND_RETURN_FALSE(ConvertSignatureToProtobufBlob(signatures,
                                                       &signature_blob));
  TEST_AND_RETURN_FALSE(AddSignatureOpToPayload(payload_path,
                                                signature_blob.size(),
                                                payload_out,
                                                metadata_size_out,
                                                signatures_offset_out));

  return true;
}

bool PayloadSigner::HashPayloadForSigning(const string& payload_path,
                                          const vector<int>& signature_sizes,
                                          brillo::Blob* out_hash_data) {
  brillo::Blob payload;
  uint64_t metadata_size;
  uint64_t signatures_offset;

  TEST_AND_RETURN_FALSE(PrepPayloadForHashing(payload_path,
                                              signature_sizes,
                                              &payload,
                                              &metadata_size,
                                              &signatures_offset));

  // Calculates the hash on the updated payload. Note that we stop calculating
  // before we reach the signature information.
  TEST_AND_RETURN_FALSE(OmahaHashCalculator::RawHashOfBytes(payload.data(),
                                                            signatures_offset,
                                                            out_hash_data));
  return true;
}

bool PayloadSigner::HashMetadataForSigning(const string& payload_path,
                                           const vector<int>& signature_sizes,
                                           brillo::Blob* out_metadata_hash) {
  brillo::Blob payload;
  uint64_t metadata_size;
  uint64_t signatures_offset;

  TEST_AND_RETURN_FALSE(PrepPayloadForHashing(payload_path,
                                              signature_sizes,
                                              &payload,
                                              &metadata_size,
                                              &signatures_offset));

  // Calculates the hash on the manifest.
  TEST_AND_RETURN_FALSE(OmahaHashCalculator::RawHashOfBytes(payload.data(),
                                                            metadata_size,
                                                            out_metadata_hash));
  return true;
}

bool PayloadSigner::AddSignatureToPayload(
    const string& payload_path,
    const vector<brillo::Blob>& signatures,
    const string& signed_payload_path,
    uint64_t *out_metadata_size) {
  // TODO(petkov): Reduce memory usage -- the payload is manipulated in memory.

  // Loads the payload and adds the signature op to it.
  brillo::Blob signature_blob;
  TEST_AND_RETURN_FALSE(ConvertSignatureToProtobufBlob(signatures,
                                                       &signature_blob));
  brillo::Blob payload;
  uint64_t signatures_offset;
  TEST_AND_RETURN_FALSE(AddSignatureOpToPayload(payload_path,
                                                signature_blob.size(),
                                                &payload,
                                                out_metadata_size,
                                                &signatures_offset));
  // Appends the signature blob to the end of the payload and writes the new
  // payload.
  LOG(INFO) << "Payload size before signatures: " << payload.size();
  payload.resize(signatures_offset);
  payload.insert(payload.begin() + signatures_offset,
                 signature_blob.begin(),
                 signature_blob.end());
  LOG(INFO) << "Signed payload size: " << payload.size();
  TEST_AND_RETURN_FALSE(utils::WriteFile(signed_payload_path.c_str(),
                                         payload.data(),
                                         payload.size()));
  return true;
}

bool PayloadSigner::GetMetadataSignature(const void* const metadata,
                                         size_t metadata_size,
                                         const string& private_key_path,
                                         string* out_signature) {
  // Calculates the hash on the updated payload. Note that the payload includes
  // the signature op but doesn't include the signature blob at the end.
  brillo::Blob metadata_hash;
  TEST_AND_RETURN_FALSE(OmahaHashCalculator::RawHashOfBytes(metadata,
                                                            metadata_size,
                                                            &metadata_hash));

  brillo::Blob signature;
  TEST_AND_RETURN_FALSE(SignHash(metadata_hash,
                                 private_key_path,
                                 &signature));

  *out_signature = brillo::data_encoding::Base64Encode(signature);
  return true;
}


}  // namespace chromeos_update_engine
