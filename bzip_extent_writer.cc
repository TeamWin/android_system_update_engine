// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/bzip_extent_writer.h"

using std::vector;

namespace chromeos_update_engine {

namespace {
const chromeos::Blob::size_type kOutputBufferLength = 16 * 1024;
}

bool BzipExtentWriter::Init(FileDescriptorPtr fd,
                            const vector<Extent>& extents,
                            uint32_t block_size) {
  // Init bzip2 stream
  int rc = BZ2_bzDecompressInit(&stream_,
                                0,   // verbosity. (0 == silent)
                                0);  // 0 = faster algo, more memory

  TEST_AND_RETURN_FALSE(rc == BZ_OK);

  return next_->Init(fd, extents, block_size);
}

bool BzipExtentWriter::Write(const void* bytes, size_t count) {
  chromeos::Blob output_buffer(kOutputBufferLength);

  // Copy the input data into |input_buffer_| only if |input_buffer_| already
  // contains unconsumed data. Otherwise, process the data directly from the
  // source.
  const uint8_t* input = reinterpret_cast<const uint8_t*>(bytes);
  const uint8_t* input_end = input + count;
  if (!input_buffer_.empty()) {
    input_buffer_.insert(input_buffer_.end(), input, input_end);
    input = input_buffer_.data();
    input_end = input + input_buffer_.size();
  }
  stream_.next_in = reinterpret_cast<char*>(const_cast<uint8_t*>(input));
  stream_.avail_in = input_end - input;

  for (;;) {
    stream_.next_out = reinterpret_cast<char*>(output_buffer.data());
    stream_.avail_out = output_buffer.size();

    int rc = BZ2_bzDecompress(&stream_);
    TEST_AND_RETURN_FALSE(rc == BZ_OK || rc == BZ_STREAM_END);

    if (stream_.avail_out == output_buffer.size())
      break;  // got no new bytes

    TEST_AND_RETURN_FALSE(
        next_->Write(output_buffer.data(),
                     output_buffer.size() - stream_.avail_out));

    if (rc == BZ_STREAM_END)
      CHECK_EQ(stream_.avail_in, 0u);
    if (stream_.avail_in == 0)
      break;  // no more input to process
  }

  // Store unconsumed data (if any) in |input_buffer_|.
  if (stream_.avail_in || !input_buffer_.empty()) {
    chromeos::Blob new_input_buffer(input_end - stream_.avail_in, input_end);
    new_input_buffer.swap(input_buffer_);
  }

  return true;
}

bool BzipExtentWriter::EndImpl() {
  TEST_AND_RETURN_FALSE(input_buffer_.empty());
  TEST_AND_RETURN_FALSE(BZ2_bzDecompressEnd(&stream_) == BZ_OK);
  return next_->End();
}

}  // namespace chromeos_update_engine
