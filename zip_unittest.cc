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

#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "update_engine/bzip.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using chromeos_update_engine::test_utils::kRandomString;
using std::string;
using std::vector;

namespace chromeos_update_engine {

template <typename T>
class ZipTest : public ::testing::Test {
 public:
  bool ZipDecompress(const chromeos::Blob& in,
                     chromeos::Blob* out) const = 0;
  bool ZipCompress(const chromeos::Blob& in,
                   chromeos::Blob* out) const = 0;
  bool ZipCompressString(const string& str,
                         chromeos::Blob* out) const = 0;
  bool ZipDecompressString(const string& str,
                           chromeos::Blob* out) const = 0;
};

class BzipTest {};

template <>
class ZipTest<BzipTest> : public ::testing::Test {
 public:
  bool ZipDecompress(const chromeos::Blob& in,
                     chromeos::Blob* out) const {
    return BzipDecompress(in, out);
  }
  bool ZipCompress(const chromeos::Blob& in,
                   chromeos::Blob* out) const {
    return BzipCompress(in, out);
  }
  bool ZipCompressString(const string& str,
                         chromeos::Blob* out) const {
    return BzipCompressString(str, out);
  }
  bool ZipDecompressString(const string& str,
                           chromeos::Blob* out) const {
    return BzipDecompressString(str, out);
  }
};

typedef ::testing::Types<BzipTest> ZipTestTypes;
TYPED_TEST_CASE(ZipTest, ZipTestTypes);



TYPED_TEST(ZipTest, SimpleTest) {
  string in("this should compress well xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
  chromeos::Blob out;
  EXPECT_TRUE(this->ZipCompressString(in, &out));
  EXPECT_LT(out.size(), in.size());
  EXPECT_GT(out.size(), 0);
  chromeos::Blob decompressed;
  EXPECT_TRUE(this->ZipDecompress(out, &decompressed));
  EXPECT_EQ(in.size(), decompressed.size());
  EXPECT_TRUE(!memcmp(in.data(), decompressed.data(), in.size()));
}

TYPED_TEST(ZipTest, PoorCompressionTest) {
  string in(reinterpret_cast<const char*>(kRandomString),
            sizeof(kRandomString));
  chromeos::Blob out;
  EXPECT_TRUE(this->ZipCompressString(in, &out));
  EXPECT_GT(out.size(), in.size());
  string out_string(out.begin(), out.end());
  chromeos::Blob decompressed;
  EXPECT_TRUE(this->ZipDecompressString(out_string, &decompressed));
  EXPECT_EQ(in.size(), decompressed.size());
  EXPECT_TRUE(!memcmp(in.data(), decompressed.data(), in.size()));
}

TYPED_TEST(ZipTest, MalformedZipTest) {
  string in(reinterpret_cast<const char*>(kRandomString),
            sizeof(kRandomString));
  chromeos::Blob out;
  EXPECT_FALSE(this->ZipDecompressString(in, &out));
}

TYPED_TEST(ZipTest, EmptyInputsTest) {
  string in;
  chromeos::Blob out;
  EXPECT_TRUE(this->ZipDecompressString(in, &out));
  EXPECT_EQ(0, out.size());

  EXPECT_TRUE(this->ZipCompressString(in, &out));
  EXPECT_EQ(0, out.size());
}

}  // namespace chromeos_update_engine
