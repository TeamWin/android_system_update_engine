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

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlRequestIdTest) {
  OmahaEvent omaha_event;
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  OmahaRequestBuilderXml omaha_request{&omaha_event,
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
  OmahaEvent omaha_event;
  OmahaRequestParams omaha_request_params{&fake_system_state_};
  OmahaRequestBuilderXml omaha_request{&omaha_event,
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

}  // namespace chromeos_update_engine
