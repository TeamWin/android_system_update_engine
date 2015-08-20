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

#include "update_engine/bzip.h"

#include <stdlib.h>
#include <algorithm>
#include <bzlib.h>
#include <limits>

#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

// BzipData compresses or decompresses the input to the output.
// Returns true on success.
// Use one of BzipBuffToBuff*ompress as the template parameter to BzipData().
int BzipBuffToBuffDecompress(uint8_t* out,
                             uint32_t* out_length,
                             const void* in,
                             uint32_t in_length) {
  return BZ2_bzBuffToBuffDecompress(
      reinterpret_cast<char*>(out),
      out_length,
      reinterpret_cast<char*>(const_cast<void*>(in)),
      in_length,
      0,  // Silent verbosity
      0);  // Normal algorithm
}

int BzipBuffToBuffCompress(uint8_t* out,
                           uint32_t* out_length,
                           const void* in,
                           uint32_t in_length) {
  return BZ2_bzBuffToBuffCompress(
      reinterpret_cast<char*>(out),
      out_length,
      reinterpret_cast<char*>(const_cast<void*>(in)),
      in_length,
      9,  // Best compression
      0,  // Silent verbosity
      0);  // Default work factor
}

template<int F(uint8_t* out,
               uint32_t* out_length,
               const void* in,
               uint32_t in_length)>
bool BzipData(const void* const in,
              const size_t in_size,
              chromeos::Blob* const out) {
  TEST_AND_RETURN_FALSE(out);
  out->clear();
  if (in_size == 0) {
    return true;
  }
  // Try increasing buffer size until it works
  size_t buf_size = in_size;
  out->resize(buf_size);

  for (;;) {
    if (buf_size > std::numeric_limits<uint32_t>::max())
      return false;
    uint32_t data_size = buf_size;
    int rc = F(out->data(), &data_size, in, in_size);
    TEST_AND_RETURN_FALSE(rc == BZ_OUTBUFF_FULL || rc == BZ_OK);
    if (rc == BZ_OK) {
      // we're done!
      out->resize(data_size);
      return true;
    }

    // Data didn't fit; double the buffer size.
    buf_size *= 2;
    out->resize(buf_size);
  }
}

}  // namespace

bool BzipDecompress(const chromeos::Blob& in, chromeos::Blob* out) {
  return BzipData<BzipBuffToBuffDecompress>(in.data(), in.size(), out);
}

bool BzipCompress(const chromeos::Blob& in, chromeos::Blob* out) {
  return BzipData<BzipBuffToBuffCompress>(in.data(), in.size(), out);
}

namespace {
template<bool F(const void* const in,
                const size_t in_size,
                chromeos::Blob* const out)>
bool BzipString(const string& str,
                chromeos::Blob* out) {
  TEST_AND_RETURN_FALSE(out);
  chromeos::Blob temp;
  TEST_AND_RETURN_FALSE(F(str.data(), str.size(), &temp));
  out->clear();
  out->insert(out->end(), temp.begin(), temp.end());
  return true;
}
}  // namespace

bool BzipCompressString(const string& str, chromeos::Blob* out) {
  return BzipString<BzipData<BzipBuffToBuffCompress>>(str, out);
}

bool BzipDecompressString(const string& str, chromeos::Blob* out) {
  return BzipString<BzipData<BzipBuffToBuffDecompress>>(str, out);
}

}  // namespace chromeos_update_engine
