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

#include "common/mock_action_processor.h"
#include <gmock/gmock-actions.h>
#include <gmock/gmock-function-mocker.h>
#include <gmock/gmock-spec-builders.h>

#include "payload_consumer/install_plan.h"
#include "update_engine/common/action_pipe.h"
#include "update_engine/common/boot_control_stub.h"
#include "update_engine/common/constants.h"
#include "update_engine/common/mock_http_fetcher.h"
#include "update_engine/common/mock_prefs.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/payload_consumer/download_action.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

namespace chromeos_update_engine {
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

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
  auto download_action =
      std::make_unique<DownloadAction>(&prefs,
                                       &boot_control,
                                       nullptr,
                                       nullptr,
                                       http_fetcher,
                                       false /* interactive */);
  download_action->set_in_pipe(action_pipe);
  MockActionProcessor mock_processor;
  download_action->SetProcessor(&mock_processor);
  download_action->PerformAction();
  ASSERT_EQ(download_action->http_fetcher()->GetBytesDownloaded(), data.size());
}

}  // namespace chromeos_update_engine
