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

#include <gtest/gtest.h>

using std::string;

namespace chromeos_update_engine {

class OmahaRequestBuilderXmlTest : public ::testing::Test {};

TEST_F(OmahaRequestBuilderXmlTest, XmlEncodeTest) {
  string output;
  EXPECT_TRUE(XmlEncode("ab", &output));
  EXPECT_EQ("ab", output);
  EXPECT_TRUE(XmlEncode("a<b", &output));
  EXPECT_EQ("a&lt;b", output);
  EXPECT_TRUE(XmlEncode("<&>\"\'\\", &output));
  EXPECT_EQ("&lt;&amp;&gt;&quot;&apos;\\", output);
  EXPECT_TRUE(XmlEncode("&lt;&amp;&gt;", &output));
  EXPECT_EQ("&amp;lt;&amp;amp;&amp;gt;", output);
  // Check that unterminated UTF-8 strings are handled properly.
  EXPECT_FALSE(XmlEncode("\xc2", &output));
  // Fail with invalid ASCII-7 chars.
  EXPECT_FALSE(XmlEncode("This is an 'n' with a tilde: \xc3\xb1", &output));
}

TEST_F(OmahaRequestBuilderXmlTest, XmlEncodeWithDefaultTest) {
  EXPECT_EQ("&lt;&amp;&gt;", XmlEncodeWithDefault("<&>", "something else"));
  EXPECT_EQ("<not escaped>", XmlEncodeWithDefault("\xc2", "<not escaped>"));
}

}  // namespace chromeos_update_engine
