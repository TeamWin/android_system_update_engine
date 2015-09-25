//
// Copyright (C) 2012 The Android Open Source Project
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

#include "update_engine/delta_performer.h"

#include <inttypes.h>

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>
#include <google/protobuf/repeated_field.h>
#include <gtest/gtest.h>

#include "update_engine/constants.h"
#include "update_engine/fake_hardware.h"
#include "update_engine/fake_system_state.h"
#include "update_engine/mock_prefs.h"
#include "update_engine/payload_constants.h"
#include "update_engine/payload_generator/payload_file.h"
#include "update_engine/payload_generator/payload_signer.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

using std::string;
using std::vector;
using testing::Return;
using testing::_;
using test_utils::kRandomString;
using test_utils::System;

extern const char* kUnittestPrivateKeyPath;
extern const char* kUnittestPublicKeyPath;

static const char* kBogusMetadataSignature1 =
    "awSFIUdUZz2VWFiR+ku0Pj00V7bPQPQFYQSXjEXr3vaw3TE4xHV5CraY3/YrZpBv"
    "J5z4dSBskoeuaO1TNC/S6E05t+yt36tE4Fh79tMnJ/z9fogBDXWgXLEUyG78IEQr"
    "YH6/eBsQGT2RJtBgXIXbZ9W+5G9KmGDoPOoiaeNsDuqHiBc/58OFsrxskH8E6vMS"
    "BmMGGk82mvgzic7ApcoURbCGey1b3Mwne/hPZ/bb9CIyky8Og9IfFMdL2uAweOIR"
    "fjoTeLYZpt+WN65Vu7jJ0cQN8e1y+2yka5112wpRf/LLtPgiAjEZnsoYpLUd7CoV"
    "pLRtClp97kN2+tXGNBQqkA==";

namespace {
// Different options that determine what we should fill into the
// install_plan.metadata_signature to simulate the contents received in the
// Omaha response.
enum MetadataSignatureTest {
  kEmptyMetadataSignature,
  kInvalidMetadataSignature,
  kValidMetadataSignature,
};

}  // namespace

class DeltaPerformerTest : public ::testing::Test {
 public:
  // Test helper placed where it can easily be friended from DeltaPerformer.
  static void RunManifestValidation(const DeltaArchiveManifest& manifest,
                                    bool full_payload,
                                    ErrorCode expected) {
    MockPrefs prefs;
    InstallPlan install_plan;
    FakeSystemState fake_system_state;
    DeltaPerformer performer(&prefs, &fake_system_state, &install_plan);

    // The install plan is for Full or Delta.
    install_plan.is_full_update = full_payload;

    // The Manifest we are validating.
    performer.manifest_.CopyFrom(manifest);

    EXPECT_EQ(expected, performer.ValidateManifest());
  }

  static chromeos::Blob GeneratePayload(const chromeos::Blob& blob_data,
                                        const vector<AnnotatedOperation>& aops,
                                        bool sign_payload,
                                        int32_t minor_version,
                                        uint64_t* out_metadata_size) {
    string blob_path;
    EXPECT_TRUE(utils::MakeTempFile("Blob-XXXXXX", &blob_path, nullptr));
    ScopedPathUnlinker blob_unlinker(blob_path);
    EXPECT_TRUE(utils::WriteFile(blob_path.c_str(),
                                 blob_data.data(),
                                 blob_data.size()));

    PayloadGenerationConfig config;
    config.major_version = kChromeOSMajorPayloadVersion;
    config.minor_version = minor_version;
    config.target.rootfs.path = blob_path;
    config.target.rootfs.size = blob_data.size();
    config.target.kernel.path = blob_path;
    config.target.kernel.size = blob_data.size();

    PayloadFile payload;
    EXPECT_TRUE(payload.Init(config));

    payload.AddPartition(config.source.rootfs, config.target.rootfs, aops);

    string payload_path;
    EXPECT_TRUE(utils::MakeTempFile("Payload-XXXXXX", &payload_path, nullptr));
    ScopedPathUnlinker payload_unlinker(payload_path);
    EXPECT_TRUE(payload.WritePayload(payload_path, blob_path,
        sign_payload ? kUnittestPrivateKeyPath : "",
        out_metadata_size));

    chromeos::Blob payload_data;
    EXPECT_TRUE(utils::ReadFile(payload_path, &payload_data));
    return payload_data;
  }
};

// Calls delta performer's Write method by pretending to pass in bytes from a
// delta file whose metadata size is actual_metadata_size and tests if all
// checks are correctly performed if the install plan contains
// expected_metadata_size and that the result of the parsing are as per
// hash_checks_mandatory flag.
void DoMetadataSizeTest(uint64_t expected_metadata_size,
                        uint64_t actual_metadata_size,
                        bool hash_checks_mandatory) {
  MockPrefs prefs;
  InstallPlan install_plan;
  install_plan.hash_checks_mandatory = hash_checks_mandatory;
  FakeSystemState fake_system_state;
  DeltaPerformer performer(&prefs, &fake_system_state, &install_plan);
  EXPECT_EQ(0, performer.Open("/dev/null", 0, 0));
  EXPECT_TRUE(performer.OpenKernel("/dev/null"));

  // Set a valid magic string and version number 1.
  EXPECT_TRUE(performer.Write("CrAU", 4));
  uint64_t version = htobe64(1);
  EXPECT_TRUE(performer.Write(&version, 8));

  install_plan.metadata_size = expected_metadata_size;
  ErrorCode error_code;
  // When filling in size in manifest, exclude the size of the 20-byte header.
  uint64_t size_in_manifest = htobe64(actual_metadata_size - 20);
  bool result = performer.Write(&size_in_manifest, 8, &error_code);
  if (expected_metadata_size == actual_metadata_size ||
      !hash_checks_mandatory) {
    EXPECT_TRUE(result);
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ(ErrorCode::kDownloadInvalidMetadataSize, error_code);
  }

  EXPECT_LT(performer.Close(), 0);
}

// Generates a valid delta file but tests the delta performer by suppling
// different metadata signatures as per omaha_metadata_signature flag and
// sees if the result of the parsing are as per hash_checks_mandatory flag.
void DoMetadataSignatureTest(MetadataSignatureTest metadata_signature_test,
                             bool sign_payload,
                             bool hash_checks_mandatory) {
  InstallPlan install_plan;

  // Loads the payload and parses the manifest.
  chromeos::Blob payload = DeltaPerformerTest::GeneratePayload(chromeos::Blob(),
      vector<AnnotatedOperation>(), sign_payload,
      DeltaPerformer::kFullPayloadMinorVersion, &install_plan.metadata_size);

  LOG(INFO) << "Payload size: " << payload.size();

  install_plan.hash_checks_mandatory = hash_checks_mandatory;

  DeltaPerformer::MetadataParseResult expected_result, actual_result;
  ErrorCode expected_error, actual_error;

  // Fill up the metadata signature in install plan according to the test.
  switch (metadata_signature_test) {
    case kEmptyMetadataSignature:
      install_plan.metadata_signature.clear();
      expected_result = DeltaPerformer::kMetadataParseError;
      expected_error = ErrorCode::kDownloadMetadataSignatureMissingError;
      break;

    case kInvalidMetadataSignature:
      install_plan.metadata_signature = kBogusMetadataSignature1;
      expected_result = DeltaPerformer::kMetadataParseError;
      expected_error = ErrorCode::kDownloadMetadataSignatureMismatch;
      break;

    case kValidMetadataSignature:
    default:
      // Set the install plan's metadata size to be the same as the one
      // in the manifest so that we pass the metadata size checks. Only
      // then we can get to manifest signature checks.
      ASSERT_TRUE(PayloadSigner::GetMetadataSignature(
          payload.data(),
          install_plan.metadata_size,
          kUnittestPrivateKeyPath,
          &install_plan.metadata_signature));
      EXPECT_FALSE(install_plan.metadata_signature.empty());
      expected_result = DeltaPerformer::kMetadataParseSuccess;
      expected_error = ErrorCode::kSuccess;
      break;
  }

  // Ignore the expected result/error if hash checks are not mandatory.
  if (!hash_checks_mandatory) {
    expected_result = DeltaPerformer::kMetadataParseSuccess;
    expected_error = ErrorCode::kSuccess;
  }

  // Create the delta performer object.
  MockPrefs prefs;
  FakeSystemState fake_system_state;
  DeltaPerformer delta_performer(&prefs,
                                 &fake_system_state,
                                 &install_plan);

  // Use the public key corresponding to the private key used above to
  // sign the metadata.
  EXPECT_TRUE(utils::FileExists(kUnittestPublicKeyPath));
  delta_performer.set_public_key_path(kUnittestPublicKeyPath);

  // Init actual_error with an invalid value so that we make sure
  // ParsePayloadMetadata properly populates it in all cases.
  actual_error = ErrorCode::kUmaReportedMax;
  actual_result = delta_performer.ParsePayloadMetadata(payload, &actual_error);

  EXPECT_EQ(expected_result, actual_result);
  EXPECT_EQ(expected_error, actual_error);

  // Check that the parsed metadata size is what's expected. This test
  // implicitly confirms that the metadata signature is valid, if required.
  EXPECT_EQ(install_plan.metadata_size, delta_performer.GetMetadataSize());
}

TEST(DeltaPerformerTest, ExtentsToByteStringTest) {
  uint64_t test[] = {1, 1, 4, 2, 0, 1};
  COMPILE_ASSERT(arraysize(test) % 2 == 0, array_size_uneven);
  const uint64_t block_size = 4096;
  const uint64_t file_length = 4 * block_size - 13;

  google::protobuf::RepeatedPtrField<Extent> extents;
  for (size_t i = 0; i < arraysize(test); i += 2) {
    Extent* extent = extents.Add();
    extent->set_start_block(test[i]);
    extent->set_num_blocks(test[i + 1]);
  }

  string expected_output = "4096:4096,16384:8192,0:4083";
  string actual_output;
  EXPECT_TRUE(DeltaPerformer::ExtentsToBsdiffPositionsString(extents,
                                                             block_size,
                                                             file_length,
                                                             &actual_output));
  EXPECT_EQ(expected_output, actual_output);
}

TEST(DeltaPerformerTest, ValidateManifestFullGoodTest) {
  // The Manifest we are validating.
  DeltaArchiveManifest manifest;
  manifest.mutable_new_kernel_info();
  manifest.mutable_new_rootfs_info();
  manifest.set_minor_version(DeltaPerformer::kFullPayloadMinorVersion);

  DeltaPerformerTest::RunManifestValidation(manifest, true,
                                            ErrorCode::kSuccess);
}

TEST(DeltaPerformerTest, ValidateManifestDeltaGoodTest) {
  // The Manifest we are validating.
  DeltaArchiveManifest manifest;
  manifest.mutable_old_kernel_info();
  manifest.mutable_old_rootfs_info();
  manifest.mutable_new_kernel_info();
  manifest.mutable_new_rootfs_info();
  manifest.set_minor_version(DeltaPerformer::kSupportedMinorPayloadVersion);

  DeltaPerformerTest::RunManifestValidation(manifest, false,
                                            ErrorCode::kSuccess);
}

TEST(DeltaPerformerTest, ValidateManifestFullUnsetMinorVersion) {
  // The Manifest we are validating.
  DeltaArchiveManifest manifest;

  DeltaPerformerTest::RunManifestValidation(manifest, true,
                                            ErrorCode::kSuccess);
}

TEST(DeltaPerformerTest, ValidateManifestDeltaUnsetMinorVersion) {
  // The Manifest we are validating.
  DeltaArchiveManifest manifest;

  DeltaPerformerTest::RunManifestValidation(
      manifest, false,
      ErrorCode::kUnsupportedMinorPayloadVersion);
}

TEST(DeltaPerformerTest, ValidateManifestFullOldKernelTest) {
  // The Manifest we are validating.
  DeltaArchiveManifest manifest;
  manifest.mutable_old_kernel_info();
  manifest.mutable_new_kernel_info();
  manifest.mutable_new_rootfs_info();
  manifest.set_minor_version(DeltaPerformer::kSupportedMinorPayloadVersion);

  DeltaPerformerTest::RunManifestValidation(
      manifest, true,
      ErrorCode::kPayloadMismatchedType);
}

TEST(DeltaPerformerTest, ValidateManifestFullOldRootfsTest) {
  // The Manifest we are validating.
  DeltaArchiveManifest manifest;
  manifest.mutable_old_rootfs_info();
  manifest.mutable_new_kernel_info();
  manifest.mutable_new_rootfs_info();
  manifest.set_minor_version(DeltaPerformer::kSupportedMinorPayloadVersion);

  DeltaPerformerTest::RunManifestValidation(
      manifest, true,
      ErrorCode::kPayloadMismatchedType);
}

TEST(DeltaPerformerTest, ValidateManifestBadMinorVersion) {
  // The Manifest we are validating.
  DeltaArchiveManifest manifest;

  // Generate a bad version number.
  manifest.set_minor_version(DeltaPerformer::kSupportedMinorPayloadVersion +
                             10000);

  DeltaPerformerTest::RunManifestValidation(
      manifest, false,
      ErrorCode::kUnsupportedMinorPayloadVersion);
}

TEST(DeltaPerformerTest, BadDeltaMagicTest) {
  MockPrefs prefs;
  InstallPlan install_plan;
  FakeSystemState fake_system_state;
  DeltaPerformer performer(&prefs, &fake_system_state, &install_plan);
  EXPECT_EQ(0, performer.Open("/dev/null", 0, 0));
  EXPECT_TRUE(performer.OpenKernel("/dev/null"));
  EXPECT_TRUE(performer.Write("junk", 4));
  EXPECT_TRUE(performer.Write("morejunk", 8));
  EXPECT_FALSE(performer.Write("morejunk", 8));
  EXPECT_LT(performer.Close(), 0);
}

TEST(DeltaPerformerTest, WriteUpdatesPayloadState) {
  MockPrefs prefs;
  InstallPlan install_plan;
  FakeSystemState fake_system_state;
  DeltaPerformer performer(&prefs, &fake_system_state, &install_plan);
  EXPECT_EQ(0, performer.Open("/dev/null", 0, 0));
  EXPECT_TRUE(performer.OpenKernel("/dev/null"));

  EXPECT_CALL(*(fake_system_state.mock_payload_state()),
              DownloadProgress(4)).Times(1);
  EXPECT_CALL(*(fake_system_state.mock_payload_state()),
              DownloadProgress(8)).Times(2);

  EXPECT_TRUE(performer.Write("junk", 4));
  EXPECT_TRUE(performer.Write("morejunk", 8));
  EXPECT_FALSE(performer.Write("morejunk", 8));
  EXPECT_LT(performer.Close(), 0);
}

TEST(DeltaPerformerTest, MissingMandatoryMetadataSizeTest) {
  DoMetadataSizeTest(0, 75456, true);
}

TEST(DeltaPerformerTest, MissingNonMandatoryMetadataSizeTest) {
  DoMetadataSizeTest(0, 123456, false);
}

TEST(DeltaPerformerTest, InvalidMandatoryMetadataSizeTest) {
  DoMetadataSizeTest(13000, 140000, true);
}

TEST(DeltaPerformerTest, InvalidNonMandatoryMetadataSizeTest) {
  DoMetadataSizeTest(40000, 50000, false);
}

TEST(DeltaPerformerTest, ValidMandatoryMetadataSizeTest) {
  DoMetadataSizeTest(85376, 85376, true);
}

TEST(DeltaPerformerTest, RunAsRootMandatoryEmptyMetadataSignatureTest) {
  DoMetadataSignatureTest(kEmptyMetadataSignature, true, true);
}

TEST(DeltaPerformerTest, RunAsRootNonMandatoryEmptyMetadataSignatureTest) {
  DoMetadataSignatureTest(kEmptyMetadataSignature, true, false);
}

TEST(DeltaPerformerTest, RunAsRootMandatoryInvalidMetadataSignatureTest) {
  DoMetadataSignatureTest(kInvalidMetadataSignature, true, true);
}

TEST(DeltaPerformerTest, RunAsRootNonMandatoryInvalidMetadataSignatureTest) {
  DoMetadataSignatureTest(kInvalidMetadataSignature, true, false);
}

TEST(DeltaPerformerTest, RunAsRootMandatoryValidMetadataSignature1Test) {
  DoMetadataSignatureTest(kValidMetadataSignature, false, true);
}

TEST(DeltaPerformerTest, RunAsRootMandatoryValidMetadataSignature2Test) {
  DoMetadataSignatureTest(kValidMetadataSignature, true, true);
}

TEST(DeltaPerformerTest, RunAsRootNonMandatoryValidMetadataSignatureTest) {
  DoMetadataSignatureTest(kValidMetadataSignature, true, false);
}

TEST(DeltaPerformerTest, UsePublicKeyFromResponse) {
  MockPrefs prefs;
  FakeSystemState fake_system_state;
  InstallPlan install_plan;
  base::FilePath key_path;

  // The result of the GetPublicKeyResponse() method is based on three things
  //
  //  1. Whether it's an official build; and
  //  2. Whether the Public RSA key to be used is in the root filesystem; and
  //  3. Whether the response has a public key
  //
  // We test all eight combinations to ensure that we only use the
  // public key in the response if
  //
  //  a. it's not an official build; and
  //  b. there is no key in the root filesystem.

  DeltaPerformer *performer = new DeltaPerformer(&prefs,
                                                 &fake_system_state,
                                                 &install_plan);
  FakeHardware* fake_hardware = fake_system_state.fake_hardware();

  string temp_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("PublicKeyFromResponseTests.XXXXXX",
                                       &temp_dir));
  string non_existing_file = temp_dir + "/non-existing";
  string existing_file = temp_dir + "/existing";
  EXPECT_EQ(0, System(base::StringPrintf("touch %s", existing_file.c_str())));

  // Non-official build, non-existing public-key, key in response -> true
  fake_hardware->SetIsOfficialBuild(false);
  performer->public_key_path_ = non_existing_file;
  install_plan.public_key_rsa = "VGVzdAo=";  // result of 'echo "Test" | base64'
  EXPECT_TRUE(performer->GetPublicKeyFromResponse(&key_path));
  EXPECT_FALSE(key_path.empty());
  EXPECT_EQ(unlink(key_path.value().c_str()), 0);
  // Same with official build -> false
  fake_hardware->SetIsOfficialBuild(true);
  EXPECT_FALSE(performer->GetPublicKeyFromResponse(&key_path));

  // Non-official build, existing public-key, key in response -> false
  fake_hardware->SetIsOfficialBuild(false);
  performer->public_key_path_ = existing_file;
  install_plan.public_key_rsa = "VGVzdAo=";  // result of 'echo "Test" | base64'
  EXPECT_FALSE(performer->GetPublicKeyFromResponse(&key_path));
  // Same with official build -> false
  fake_hardware->SetIsOfficialBuild(true);
  EXPECT_FALSE(performer->GetPublicKeyFromResponse(&key_path));

  // Non-official build, non-existing public-key, no key in response -> false
  fake_hardware->SetIsOfficialBuild(false);
  performer->public_key_path_ = non_existing_file;
  install_plan.public_key_rsa = "";
  EXPECT_FALSE(performer->GetPublicKeyFromResponse(&key_path));
  // Same with official build -> false
  fake_hardware->SetIsOfficialBuild(true);
  EXPECT_FALSE(performer->GetPublicKeyFromResponse(&key_path));

  // Non-official build, existing public-key, no key in response -> false
  fake_hardware->SetIsOfficialBuild(false);
  performer->public_key_path_ = existing_file;
  install_plan.public_key_rsa = "";
  EXPECT_FALSE(performer->GetPublicKeyFromResponse(&key_path));
  // Same with official build -> false
  fake_hardware->SetIsOfficialBuild(true);
  EXPECT_FALSE(performer->GetPublicKeyFromResponse(&key_path));

  // Non-official build, non-existing public-key, key in response
  // but invalid base64 -> false
  fake_hardware->SetIsOfficialBuild(false);
  performer->public_key_path_ = non_existing_file;
  install_plan.public_key_rsa = "not-valid-base64";
  EXPECT_FALSE(performer->GetPublicKeyFromResponse(&key_path));

  delete performer;
  EXPECT_TRUE(test_utils::RecursiveUnlinkDir(temp_dir));
}

TEST(DeltaPerformerTest, MinorVersionsMatch) {
  // Test that the minor version in update_engine.conf that is installed to
  // the image matches the supported delta minor version in the update engine.
  uint32_t minor_version;
  chromeos::KeyValueStore store;
  EXPECT_TRUE(store.Load(base::FilePath("update_engine.conf")));
  EXPECT_TRUE(utils::GetMinorVersion(store, &minor_version));
  EXPECT_EQ(DeltaPerformer::kSupportedMinorPayloadVersion, minor_version);
}

}  // namespace chromeos_update_engine
