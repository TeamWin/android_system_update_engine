// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <unistd.h>

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/strings/stringprintf.h>

#include "update_engine/policy_manager/real_random_provider.h"
#include "update_engine/policy_manager/variable.h"

using std::string;

namespace {

// The device providing randomness.
const char* kRandomDevice = "/dev/urandom";

}  // namespace

namespace chromeos_policy_manager {

// A random seed variable.
class RandomSeedVariable : public Variable<uint64_t> {
 public:
  // RandomSeedVariable is initialized as kVariableModeConst to let the
  // EvaluationContext cache the value between different evaluations of the same
  // policy request.
  RandomSeedVariable(const string& name, FILE* fp)
      : Variable<uint64_t>(name, kVariableModeConst), fp_(fp) {}
  virtual ~RandomSeedVariable() {}

 protected:
  virtual const uint64_t* GetValue(base::TimeDelta /* timeout */,
                                   string* errmsg) {
    uint64_t result;
    // Aliasing via char pointer abides by the C/C++ strict-aliasing rules.
    char* const buf = reinterpret_cast<char*>(&result);
    unsigned int buf_rd = 0;

    while (buf_rd < sizeof(result)) {
      int rd = fread(buf + buf_rd, 1, sizeof(result) - buf_rd, fp_.get());
      if (rd == 0 || ferror(fp_.get())) {
        // Either EOF on fp or read failed.
        if (errmsg) {
          *errmsg = base::StringPrintf(
              "Error reading from the random device: %s", kRandomDevice);
        }
        return NULL;
      }
      buf_rd += rd;
    }

    return new uint64_t(result);
  }

 private:
  base::ScopedFILE fp_;

  DISALLOW_COPY_AND_ASSIGN(RandomSeedVariable);
};

bool RealRandomProvider::Init(void) {
  FILE* fp = fopen(kRandomDevice, "r");
  if (!fp)
    return false;
  var_seed_.reset(new RandomSeedVariable("seed", fp));
  return true;
}

}  // namespace chromeos_policy_manager
