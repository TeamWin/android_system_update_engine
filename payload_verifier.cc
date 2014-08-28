// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_verifier.h"

#include <base/logging.h>
#include <openssl/pem.h>

#include "update_engine/delta_performer.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

const uint32_t kSignatureMessageOriginalVersion = 1;
const uint32_t kSignatureMessageCurrentVersion = 1;

namespace {

// The following is a standard PKCS1-v1_5 padding for SHA256 signatures, as
// defined in RFC3447. It is prepended to the actual signature (32 bytes) to
// form a sequence of 256 bytes (2048 bits) that is amenable to RSA signing. The
// padded hash will look as follows:
//
//    0x00 0x01 0xff ... 0xff 0x00  ASN1HEADER  SHA256HASH
//   |--------------205-----------||----19----||----32----|
//
// where ASN1HEADER is the ASN.1 description of the signed data. The complete 51
// bytes of actual data (i.e. the ASN.1 header complete with the hash) are
// packed as follows:
//
//  SEQUENCE(2+49) {
//   SEQUENCE(2+13) {
//    OBJECT(2+9) id-sha256
//    NULL(2+0)
//   }
//   OCTET STRING(2+32) <actual signature bytes...>
//  }
const unsigned char kRSA2048SHA256Padding[] = {
  // PKCS1-v1_5 padding
  0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x00,
  // ASN.1 header
  0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
  0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
  0x00, 0x04, 0x20,
};

}  // namespace

bool PayloadVerifier::LoadPayload(const string& payload_path,
                                  vector<char>* out_payload,
                                  DeltaArchiveManifest* out_manifest,
                                  uint64_t* out_metadata_size) {
  vector<char> payload;
  // Loads the payload and parses the manifest.
  TEST_AND_RETURN_FALSE(utils::ReadFile(payload_path, &payload));
  LOG(INFO) << "Payload size: " << payload.size();
  ErrorCode error = ErrorCode::kSuccess;
  InstallPlan install_plan;
  DeltaPerformer delta_performer(nullptr, nullptr, &install_plan);
  TEST_AND_RETURN_FALSE(
      delta_performer.ParsePayloadMetadata(payload, &error) ==
      DeltaPerformer::kMetadataParseSuccess);
  TEST_AND_RETURN_FALSE(delta_performer.GetManifest(out_manifest));
  *out_metadata_size = delta_performer.GetMetadataSize();
  LOG(INFO) << "Metadata size: " << *out_metadata_size;
  out_payload->swap(payload);
  return true;
}

bool PayloadVerifier::VerifySignature(const std::vector<char>& signature_blob,
                                      const std::string& public_key_path,
                                      std::vector<char>* out_hash_data) {
  return VerifySignatureBlob(signature_blob, public_key_path,
                             kSignatureMessageCurrentVersion, out_hash_data);
}

bool PayloadVerifier::VerifySignatureBlob(
    const std::vector<char>& signature_blob,
    const std::string& public_key_path,
    uint32_t client_version,
    std::vector<char>* out_hash_data) {
  TEST_AND_RETURN_FALSE(!public_key_path.empty());

  Signatures signatures;
  LOG(INFO) << "signature size = " <<  signature_blob.size();
  TEST_AND_RETURN_FALSE(signatures.ParseFromArray(&signature_blob[0],
                                                  signature_blob.size()));

  // Finds a signature that matches the current version.
  int sig_index = 0;
  for (; sig_index < signatures.signatures_size(); sig_index++) {
    const Signatures_Signature& signature = signatures.signatures(sig_index);
    if (signature.has_version() &&
        signature.version() == client_version) {
      break;
    }
  }
  TEST_AND_RETURN_FALSE(sig_index < signatures.signatures_size());

  const Signatures_Signature& signature = signatures.signatures(sig_index);
  vector<char> sig_data(signature.data().begin(), signature.data().end());

  return GetRawHashFromSignature(sig_data, public_key_path, out_hash_data);
}


bool PayloadVerifier::GetRawHashFromSignature(
    const std::vector<char>& sig_data,
    const std::string& public_key_path,
    std::vector<char>* out_hash_data) {
  TEST_AND_RETURN_FALSE(!public_key_path.empty());

  // The code below executes the equivalent of:
  //
  // openssl rsautl -verify -pubin -inkey |public_key_path|
  //   -in |sig_data| -out |out_hash_data|

  // Loads the public key.
  FILE* fpubkey = fopen(public_key_path.c_str(), "rb");
  if (!fpubkey) {
    LOG(ERROR) << "Unable to open public key file: " << public_key_path;
    return false;
  }

  char dummy_password[] = { ' ', 0 };  // Ensure no password is read from stdin.
  RSA* rsa = PEM_read_RSA_PUBKEY(fpubkey, nullptr, nullptr, dummy_password);
  fclose(fpubkey);
  TEST_AND_RETURN_FALSE(rsa != nullptr);
  unsigned int keysize = RSA_size(rsa);
  if (sig_data.size() > 2 * keysize) {
    LOG(ERROR) << "Signature size is too big for public key size.";
    RSA_free(rsa);
    return false;
  }

  // Decrypts the signature.
  vector<char> hash_data(keysize);
  int decrypt_size = RSA_public_decrypt(
      sig_data.size(),
      reinterpret_cast<const unsigned char*>(sig_data.data()),
      reinterpret_cast<unsigned char*>(hash_data.data()),
      rsa,
      RSA_NO_PADDING);
  RSA_free(rsa);
  TEST_AND_RETURN_FALSE(decrypt_size > 0 &&
                        decrypt_size <= static_cast<int>(hash_data.size()));
  hash_data.resize(decrypt_size);
  out_hash_data->swap(hash_data);
  return true;
}

bool PayloadVerifier::VerifySignedPayload(const std::string& payload_path,
                                          const std::string& public_key_path,
                                          uint32_t client_key_check_version) {
  vector<char> payload;
  DeltaArchiveManifest manifest;
  uint64_t metadata_size;
  TEST_AND_RETURN_FALSE(LoadPayload(
      payload_path, &payload, &manifest, &metadata_size));
  TEST_AND_RETURN_FALSE(manifest.has_signatures_offset() &&
                        manifest.has_signatures_size());
  CHECK_EQ(payload.size(),
           metadata_size + manifest.signatures_offset() +
           manifest.signatures_size());
  vector<char> signature_blob(
      payload.begin() + metadata_size + manifest.signatures_offset(),
      payload.end());
  vector<char> signed_hash;
  TEST_AND_RETURN_FALSE(VerifySignatureBlob(
      signature_blob, public_key_path, client_key_check_version, &signed_hash));
  TEST_AND_RETURN_FALSE(!signed_hash.empty());
  vector<char> hash;
  TEST_AND_RETURN_FALSE(OmahaHashCalculator::RawHashOfBytes(
      payload.data(), metadata_size + manifest.signatures_offset(), &hash));
  PadRSA2048SHA256Hash(&hash);
  TEST_AND_RETURN_FALSE(hash == signed_hash);
  return true;
}

bool PayloadVerifier::PadRSA2048SHA256Hash(std::vector<char>* hash) {
  TEST_AND_RETURN_FALSE(hash->size() == 32);
  hash->insert(hash->begin(),
               reinterpret_cast<const char*>(kRSA2048SHA256Padding),
               reinterpret_cast<const char*>(kRSA2048SHA256Padding +
                                             sizeof(kRSA2048SHA256Padding)));
  TEST_AND_RETURN_FALSE(hash->size() == 256);
  return true;
}

}  // namespace chromeos_update_engine
