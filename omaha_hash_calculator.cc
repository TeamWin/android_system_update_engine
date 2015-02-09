// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_hash_calculator.h"

#include <fcntl.h>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <chromeos/data_encoding.h>

#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

OmahaHashCalculator::OmahaHashCalculator() : valid_(false) {
  valid_ = (SHA256_Init(&ctx_) == 1);
  LOG_IF(ERROR, !valid_) << "SHA256_Init failed";
}

// Update is called with all of the data that should be hashed in order.
// Mostly just passes the data through to OpenSSL's SHA256_Update()
bool OmahaHashCalculator::Update(const char* data, size_t length) {
  TEST_AND_RETURN_FALSE(valid_);
  TEST_AND_RETURN_FALSE(hash_.empty());
  static_assert(sizeof(size_t) <= sizeof(unsigned long),  // NOLINT(runtime/int)
                "length param may be truncated in SHA256_Update");
  TEST_AND_RETURN_FALSE(SHA256_Update(&ctx_, data, length) == 1);
  return true;
}

off_t OmahaHashCalculator::UpdateFile(const string& name, off_t length) {
  int fd = HANDLE_EINTR(open(name.c_str(), O_RDONLY));
  if (fd < 0) {
    return -1;
  }

  const int kBufferSize = 128 * 1024;  // 128 KiB
  vector<char> buffer(kBufferSize);
  off_t bytes_processed = 0;
  while (length < 0 || bytes_processed < length) {
    off_t bytes_to_read = buffer.size();
    if (length >= 0 && bytes_to_read > length - bytes_processed) {
      bytes_to_read = length - bytes_processed;
    }
    ssize_t rc = HANDLE_EINTR(read(fd, buffer.data(), bytes_to_read));
    if (rc == 0) {  // EOF
      break;
    }
    if (rc < 0 || !Update(buffer.data(), rc)) {
      bytes_processed = -1;
      break;
    }
    bytes_processed += rc;
  }
  IGNORE_EINTR(close(fd));
  return bytes_processed;
}

// Call Finalize() when all data has been passed in. This mostly just
// calls OpenSSL's SHA256_Final() and then base64 encodes the hash.
bool OmahaHashCalculator::Finalize() {
  TEST_AND_RETURN_FALSE(hash_.empty());
  TEST_AND_RETURN_FALSE(raw_hash_.empty());
  raw_hash_.resize(SHA256_DIGEST_LENGTH);
  TEST_AND_RETURN_FALSE(
      SHA256_Final(reinterpret_cast<unsigned char*>(&raw_hash_[0]),
                   &ctx_) == 1);

  // Convert raw_hash_ to base64 encoding and store it in hash_.
  hash_ = chromeos::data_encoding::Base64Encode(raw_hash_.data(),
                                                raw_hash_.size());
  return true;
}

bool OmahaHashCalculator::RawHashOfBytes(const char* data,
                                         size_t length,
                                         vector<char>* out_hash) {
  OmahaHashCalculator calc;
  TEST_AND_RETURN_FALSE(calc.Update(data, length));
  TEST_AND_RETURN_FALSE(calc.Finalize());
  *out_hash = calc.raw_hash();
  return true;
}

bool OmahaHashCalculator::RawHashOfData(const vector<char>& data,
                                        vector<char>* out_hash) {
  return RawHashOfBytes(data.data(), data.size(), out_hash);
}

off_t OmahaHashCalculator::RawHashOfFile(const string& name, off_t length,
                                         vector<char>* out_hash) {
  OmahaHashCalculator calc;
  off_t res = calc.UpdateFile(name, length);
  if (res < 0) {
    return res;
  }
  if (!calc.Finalize()) {
    return -1;
  }
  *out_hash = calc.raw_hash();
  return res;
}

string OmahaHashCalculator::OmahaHashOfBytes(
    const void* data, size_t length) {
  OmahaHashCalculator calc;
  calc.Update(reinterpret_cast<const char*>(data), length);
  calc.Finalize();
  return calc.hash();
}

string OmahaHashCalculator::OmahaHashOfString(const string& str) {
  return OmahaHashOfBytes(str.data(), str.size());
}

string OmahaHashCalculator::OmahaHashOfData(const vector<char>& data) {
  return OmahaHashOfBytes(&data[0], data.size());
}

string OmahaHashCalculator::GetContext() const {
  return string(reinterpret_cast<const char*>(&ctx_), sizeof(ctx_));
}

bool OmahaHashCalculator::SetContext(const string& context) {
  TEST_AND_RETURN_FALSE(context.size() == sizeof(ctx_));
  memcpy(&ctx_, context.data(), sizeof(ctx_));
  return true;
}

}  // namespace chromeos_update_engine
