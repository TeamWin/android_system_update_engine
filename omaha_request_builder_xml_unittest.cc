//
// Copyright (C) 2019 The Android Open Source Project
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

#include "update_engine/omaha_request_builder_xml.h"

#include <string>
#include <utility>
#include <vector>

#include <base/guid.h>
#include <gtest/gtest.h>

#include "update_engine/fake_system_state.h"

using std::pair;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
// Helper to find key and extract value from the given string |xml|, instead
// of using a full parser. The attribute key will be followed by "=\"" as xml
// attribute values must be within double quotes (not single quotes).
static string FindAttributeKeyValueInXml(const string& xml,
                                         const string& key,
                                         const size_t val_size) {
  string key_with_quotes = key + "=\"";
  const size_t val_start_pos = xml.find(key);
  if (val_start_pos == string::npos)
    return "";
  return xml.substr(val_start_pos + key_with_quotes.size(), val_size);
}
// Helper to find the count of substring in a string.
static size_t CountSubstringInString(const string& str, const string& substr) {
  size_t count = 0, pos = 0;
  while ((pos = str.find(substr, pos ? pos + 1 : 0)) != string::npos)
    ++count;
  return count;
}
}  // namespace

class OmahaRequestBuilderXmlTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}

  FakeSystemState fake_system_state_;
  static constexpr size_t kGuidSize = 36;
};

TEST_F(OmahaRequestBuilderXmlTest, XmlEncodeTest) {
  string output;
  vector<pair<string, string>> xml_encode_pairs = {
      {"ab", "ab"},
      {"a<b", "a&lt;b"},
      {"<&>\"\'\\", "&lt;&amp;&gt;&quot;&apos;\\"},
      {"&lt;&amp;&gt;", "&amp;lt;&amp;amp;&amp;gt;"}};
  for (const auto& xml_encode_pair : xml_encode_pairs) {
    const auto& before_encoding = xml_encode_pair.first;
    const auto& after_encoding = xml_encode_pair.second;
    EXPECT_TRUE(XmlEncode(before_encoding, &output));
    EXPECT_EQ(after_encoding, output);
  }
  // Check that unterminated UTF-8 strings are handled properly.
  EXPECT_FALSE(XmlEncode("\xc2", &output));
  // Fail with invalid ASCII-7 chars.
  EXPECT_FALSE(XmlEncode("This is an 'n' with a tilde: \xc3\xb1", &output));
}

TEST_F(OmahaRequestBuilderXmlTest, XmlEncodeWithDefaultTest) {
  EXPECT_EQ("", XmlEncodeWithDefault(""));
  EXPECT_EQ("&lt;&amp;&gt;", XmlEncodeWithDefault("<&>", "something else"));
  EXPECT_EQ("<not escaped>", XmlEncodeWithDefault("\xc2", "<not escaped>"));
}

TEST_F(OmahaRequestBuilderXmlTest, PlatformGetAppTest) {
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  omaha_request_params.set_device_requisition("device requisition");
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       &omaha_request_params,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       fake_system_state_.prefs(),
                                       ""};
  OmahaAppData dlc_app_data = {.id = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX",
                               .version = "",
                               .skip_update = false,
                               .is_dlc = false};

  // Verify that the attributes that shouldn't be missing for Platform AppID are
  // in fact present in the <app ...></app>.
  const string app = omaha_request.GetApp(dlc_app_data);
  EXPECT_NE(string::npos, app.find("lang="));
  EXPECT_NE(string::npos, app.find("fw_version="));
  EXPECT_NE(string::npos, app.find("ec_version="));
  EXPECT_NE(string::npos, app.find("requisition="));
}

TEST_F(OmahaRequestBuilderXmlTest, DlcGetAppTest) {
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  omaha_request_params.set_device_requisition("device requisition");
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       &omaha_request_params,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       fake_system_state_.prefs(),
                                       ""};
  OmahaAppData dlc_app_data = {
      .id = "_dlc_id", .version = "", .skip_update = false, .is_dlc = true};

  // Verify that the attributes that should be missing for DLC AppIDs are in
  // fact not present in the <app ...></app>.
  const string app = omaha_request.GetApp(dlc_app_data);
  EXPECT_EQ(string::npos, app.find("lang="));
  EXPECT_EQ(string::npos, app.find("fw_version="));
  EXPECT_EQ(string::npos, app.find("ec_version="));
  EXPECT_EQ(string::npos, app.find("requisition="));
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlRequestIdTest) {
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       &omaha_request_params,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       fake_system_state_.prefs(),
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  const string key = "requestid";
  const string request_id =
      FindAttributeKeyValueInXml(request_xml, key, kGuidSize);
  // A valid |request_id| is either a GUID version 4 or empty string.
  if (!request_id.empty())
    EXPECT_TRUE(base::IsValidGUID(request_id));
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlSessionIdTest) {
  const string gen_session_id = base::GenerateGUID();
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       &omaha_request_params,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       fake_system_state_.prefs(),
                                       gen_session_id};
  const string request_xml = omaha_request.GetRequest();
  const string key = "sessionid";
  const string session_id =
      FindAttributeKeyValueInXml(request_xml, key, kGuidSize);
  // A valid |session_id| is either a GUID version 4 or empty string.
  if (!session_id.empty()) {
    EXPECT_TRUE(base::IsValidGUID(session_id));
  }
  EXPECT_EQ(gen_session_id, session_id);
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlPlatformUpdateTest) {
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       &omaha_request_params,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       fake_system_state_.prefs(),
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(1, CountSubstringInString(request_xml, "<updatecheck"))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlPlatformUpdateWithDlcsTest) {
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  omaha_request_params.set_dlc_apps_params(
      {{omaha_request_params.GetDlcAppId("dlc_no_0"), {.name = "dlc_no_0"}},
       {omaha_request_params.GetDlcAppId("dlc_no_1"), {.name = "dlc_no_1"}}});
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       &omaha_request_params,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       fake_system_state_.prefs(),
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(3, CountSubstringInString(request_xml, "<updatecheck"))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcInstallationTest) {
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  const std::map<std::string, OmahaRequestParams::AppParams> dlcs = {
      {omaha_request_params.GetDlcAppId("dlc_no_0"), {.name = "dlc_no_0"}},
      {omaha_request_params.GetDlcAppId("dlc_no_1"), {.name = "dlc_no_1"}}};
  omaha_request_params.set_dlc_apps_params(dlcs);
  omaha_request_params.set_is_install(true);
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       &omaha_request_params,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       fake_system_state_.prefs(),
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(2, CountSubstringInString(request_xml, "<updatecheck"))
      << request_xml;

  auto FindAppId = [request_xml](size_t pos) -> size_t {
    return request_xml.find("<app appid", pos);
  };
  // Skip over the Platform AppID, which is always first.
  size_t pos = FindAppId(0);
  for (auto&& _ : dlcs) {
    (void)_;
    EXPECT_NE(string::npos, (pos = FindAppId(pos + 1))) << request_xml;
    const string dlc_app_id_version = FindAttributeKeyValueInXml(
        request_xml.substr(pos), "version", string(kNoVersion).size());
    EXPECT_EQ(kNoVersion, dlc_app_id_version);

    const string false_str = "false";
    const string dlc_app_id_delta_okay = FindAttributeKeyValueInXml(
        request_xml.substr(pos), "delta_okay", false_str.length());
    EXPECT_EQ(false_str, dlc_app_id_delta_okay);
  }
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcNoPing) {
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  omaha_request_params.set_dlc_apps_params(
      {{omaha_request_params.GetDlcAppId("dlc_no_0"), {.name = "dlc_no_0"}}});
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       &omaha_request_params,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       fake_system_state_.prefs(),
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(0, CountSubstringInString(request_xml, "<ping")) << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcPingRollCallNoActive) {
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  omaha_request_params.set_dlc_apps_params(
      {{omaha_request_params.GetDlcAppId("dlc_no_0"),
        {.active_counting_type = OmahaRequestParams::kDateBased,
         .name = "dlc_no_0",
         .ping_date_last_active = 25,
         .ping_date_last_rollcall = 36,
         .send_ping = true}}});
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       &omaha_request_params,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       fake_system_state_.prefs(),
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(1, CountSubstringInString(request_xml, "<ping rd=\"36\""))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcPingRollCallAndActive) {
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  omaha_request_params.set_dlc_apps_params(
      {{omaha_request_params.GetDlcAppId("dlc_no_0"),
        {.active_counting_type = OmahaRequestParams::kDateBased,
         .name = "dlc_no_0",
         .ping_active = 1,
         .ping_date_last_active = 25,
         .ping_date_last_rollcall = 36,
         .send_ping = true}}});
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       &omaha_request_params,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       fake_system_state_.prefs(),
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(1,
            CountSubstringInString(request_xml,
                                   "<ping active=\"1\" ad=\"25\" rd=\"36\""))
      << request_xml;
}
}  // namespace chromeos_update_engine
