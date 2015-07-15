// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_OMAHA_HASH_CALCULATOR_H_
#define UPDATE_ENGINE_OMAHA_HASH_CALCULATOR_H_

#include <openssl/sha.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>
#include <chromeos/secure_blob.h>

// Omaha uses base64 encoded SHA-256 as the hash. This class provides a simple
// wrapper around OpenSSL providing such a formatted hash of data passed in.
// The methods of this class must be called in a very specific order: First the
// ctor (of course), then 0 or more calls to Update(), then Finalize(), then 0
// or more calls to hash().

namespace chromeos_update_engine {

class OmahaHashCalculator {
 public:
  OmahaHashCalculator();

  // Update is called with all of the data that should be hashed in order.
  // Update will read |length| bytes of |data|.
  // Returns true on success.
  bool Update(const void* data, size_t length);

  // Updates the hash with up to |length| bytes of data from |file|. If |length|
  // is negative, reads in and updates with the whole file. Returns the number
  // of bytes that the hash was updated with, or -1 on error.
  off_t UpdateFile(const std::string& name, off_t length);

  // Call Finalize() when all data has been passed in. This method tells
  // OpenSSl that no more data will come in and base64 encodes the resulting
  // hash.
  // Returns true on success.
  bool Finalize();

  // Gets the hash. Finalize() must have been called.
  const std::string& hash() const {
    DCHECK(!hash_.empty()) << "Call Finalize() first";
    return hash_;
  }

  const chromeos::Blob& raw_hash() const {
    DCHECK(!raw_hash_.empty()) << "Call Finalize() first";
    return raw_hash_;
  }

  // Gets the current hash context. Note that the string will contain binary
  // data (including \0 characters).
  std::string GetContext() const;

  // Sets the current hash context. |context| must the string returned by a
  // previous OmahaHashCalculator::GetContext method call. Returns true on
  // success, and false otherwise.
  bool SetContext(const std::string& context);

  static bool RawHashOfBytes(const void* data,
                             size_t length,
                             chromeos::Blob* out_hash);
  static bool RawHashOfData(const chromeos::Blob& data,
                            chromeos::Blob* out_hash);
  static off_t RawHashOfFile(const std::string& name, off_t length,
                             chromeos::Blob* out_hash);

  // Used by tests
  static std::string OmahaHashOfBytes(const void* data, size_t length);
  static std::string OmahaHashOfString(const std::string& str);
  static std::string OmahaHashOfData(const chromeos::Blob& data);

 private:
  // If non-empty, the final base64 encoded hash and the raw hash. Will only be
  // set to non-empty when Finalize is called.
  std::string hash_;
  chromeos::Blob raw_hash_;

  // Init success
  bool valid_;

  // The hash state used by OpenSSL
  SHA256_CTX ctx_;
  DISALLOW_COPY_AND_ASSIGN(OmahaHashCalculator);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_OMAHA_HASH_CALCULATOR_H_
