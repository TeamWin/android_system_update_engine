//
// Copyright (C) 2020 The Android Open Source Project
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

#include <unistd.h>
#include <cstdint>
#include <memory>

#include <gmock/gmock.h>
#include <gmock/gmock-actions.h>
#include <gmock/gmock-function-mocker.h>
#include <gmock/gmock-spec-builders.h>
#include <gtest/gtest.h>

#include "update_engine/common/action_pipe.h"
#include "update_engine/common/boot_control_stub.h"
#include "update_engine/common/constants.h"
#include "update_engine/common/download_action.h"
#include "update_engine/common/fake_hardware.h"
#include "update_engine/common/mock_action_processor.h"
#include "update_engine/common/mock_http_fetcher.h"
#include "update_engine/common/mock_prefs.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/install_plan.h"
#include "update_engine/payload_consumer/payload_constants.h"
#include "update_engine/payload_generator/payload_file.h"
#include "update_engine/payload_generator/payload_signer.h"

namespace chromeos_update_engine {
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

extern const char* kUnittestPrivateKeyPath;
extern const char* kUnittestPublicKeyPath;

class DownloadActionTest : public ::testing::Test {
 public:
  static constexpr int64_t METADATA_SIZE = 1024;
  static constexpr int64_t SIGNATURE_SIZE = 256;
  std::shared_ptr<ActionPipe<InstallPlan>> action_pipe{
      new ActionPipe<InstallPlan>()};
};

TEST_F(DownloadActionTest, CacheManifestInvalid) {
  std::string data(METADATA_SIZE + SIGNATURE_SIZE, '-');
  MockPrefs prefs;
  EXPECT_CALL(prefs, GetInt64(kPrefsUpdateStatePayloadIndex, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0L), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsManifestMetadataSize, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(METADATA_SIZE), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsManifestSignatureSize, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(SIGNATURE_SIZE), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsUpdateStateNextDataOffset, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0L), Return(true)));
  EXPECT_CALL(prefs, GetString(kPrefsManifestBytes, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(data), Return(true)));

  BootControlStub boot_control;
  MockHttpFetcher* http_fetcher =
      new MockHttpFetcher(data.data(), data.size(), nullptr);
  http_fetcher->set_delay(false);
  InstallPlan install_plan;
  auto& payload = install_plan.payloads.emplace_back();
  install_plan.download_url = "http://fake_url.invalid";
  payload.size = data.size();
  payload.payload_urls.emplace_back("http://fake_url.invalid");
  install_plan.is_resume = true;
  action_pipe->set_contents(install_plan);

  // takes ownership of passed in HttpFetcher
  auto download_action = std::make_unique<DownloadAction>(
      &prefs, &boot_control, nullptr, http_fetcher, false /* interactive */);
  download_action->set_in_pipe(action_pipe);
  MockActionProcessor mock_processor;
  download_action->SetProcessor(&mock_processor);
  download_action->PerformAction();
  ASSERT_EQ(download_action->http_fetcher()->GetBytesDownloaded(), data.size());
}

TEST_F(DownloadActionTest, CacheManifestValid) {
  // Create a valid manifest
  PayloadGenerationConfig config;
  config.version.major = kMaxSupportedMajorPayloadVersion;
  config.version.minor = kMaxSupportedMinorPayloadVersion;

  PayloadFile payload_file;
  ASSERT_TRUE(payload_file.Init(config));
  PartitionConfig partition_config{"system"};
  ScopedTempFile partition_file("part-system-XXXXXX", true);
  ftruncate(partition_file.fd(), 4096);
  partition_config.size = 4096;
  partition_config.path = partition_file.path();
  ASSERT_TRUE(
      payload_file.AddPartition(partition_config, partition_config, {}, {}, 0));
  ScopedTempFile blob_file("Blob-XXXXXX");
  ScopedTempFile manifest_file("Manifest-XXXXXX");
  uint64_t metadata_size;
  std::string private_key =
      test_utils::GetBuildArtifactsPath(kUnittestPrivateKeyPath);
  payload_file.WritePayload(
      manifest_file.path(), blob_file.path(), private_key, &metadata_size);
  uint64_t signature_blob_length = 0;
  ASSERT_TRUE(PayloadSigner::SignatureBlobLength({private_key},
                                                 &signature_blob_length));
  std::string data;
  ASSERT_TRUE(utils::ReadFile(manifest_file.path(), &data));
  data.resize(metadata_size + signature_blob_length);

  // Setup the prefs so that manifest is cached
  MockPrefs prefs;
  EXPECT_CALL(prefs, GetInt64(kPrefsUpdateStatePayloadIndex, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0L), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsManifestMetadataSize, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(metadata_size), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsManifestSignatureSize, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<1>(signature_blob_length), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsUpdateStateNextDataOffset, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0L), Return(true)));
  EXPECT_CALL(prefs, GetString(kPrefsManifestBytes, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(data), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsUpdateStateNextOperation, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsUpdateStatePayloadIndex, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return(true)));

  BootControlStub boot_control;
  MockHttpFetcher* http_fetcher =
      new MockHttpFetcher(data.data(), data.size(), nullptr);
  http_fetcher->set_delay(false);
  InstallPlan install_plan;
  auto& payload = install_plan.payloads.emplace_back();
  install_plan.download_url = "http://fake_url.invalid";
  payload.size = data.size();
  payload.payload_urls.emplace_back("http://fake_url.invalid");
  install_plan.is_resume = true;
  action_pipe->set_contents(install_plan);

  // takes ownership of passed in HttpFetcher
  auto download_action = std::make_unique<DownloadAction>(
      &prefs, &boot_control, nullptr, http_fetcher, false /* interactive */);

  FakeHardware hardware;
  auto delta_performer = std::make_unique<DeltaPerformer>(&prefs,
                                                          &boot_control,
                                                          &hardware,
                                                          nullptr,
                                                          &install_plan,
                                                          &payload,
                                                          false);
  delta_performer->set_public_key_path(kUnittestPublicKeyPath);
  download_action->SetTestFileWriter(std::move(delta_performer));
  download_action->set_in_pipe(action_pipe);
  MockActionProcessor mock_processor;
  download_action->SetProcessor(&mock_processor);
  download_action->PerformAction();

  // Manifest is cached, so no data should be downloaded from http fetcher.
  ASSERT_EQ(download_action->http_fetcher()->GetBytesDownloaded(), 0UL);
}
}  // namespace chromeos_update_engine
