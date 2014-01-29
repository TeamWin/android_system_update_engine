// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <unistd.h>

#include <base/file_path.h>
#include <base/file_util.h>

#include "policy_manager/random_provider.h"

using std::string;

namespace {

// The device providing randomness.
const char* kRandomDevice = "/dev/urandom";

}  // namespace

namespace chromeos_policy_manager {

// The random variable class implementation.
class RandomVariable : public Variable<uint64> {
 public:
  RandomVariable(FILE* fp) : fp_(fp) {}
  virtual ~RandomVariable() {}

 protected:
  virtual const uint64* GetValue(base::TimeDelta /* timeout */,
                                 string* errmsg) {
    uint8 buf[sizeof(uint64)];
    unsigned int buf_rd = 0;

    while (buf_rd < sizeof(uint64)) {
      int rd = fread(buf + buf_rd, 1, sizeof(uint64) - buf_rd, fp_.get());
      if (rd == 0 || ferror(fp_.get())) {
        // Either EOF on fp or read failed.
        if (errmsg)
          *errmsg = string("Error reading from the random device: ")
              + kRandomDevice;
        return NULL;
      }
      buf_rd += rd;
    }
    // Convert the result to a uint64.
    uint64 result = 0;
    for (unsigned int i = 0; i < sizeof(uint64); ++i)
      result = (result << 8) | buf[i];

    return new uint64(result);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RandomVariable);

  file_util::ScopedFILE fp_;
};


// RandomProvider implementation.

bool RandomProvider::DoInit(void) {
  FILE* fp = fopen(kRandomDevice, "r");
  if (!fp)
    return false;
  var_random_seed = new RandomVariable(fp);
  return true;
}

}  // namespace chromeos_policy_manager
