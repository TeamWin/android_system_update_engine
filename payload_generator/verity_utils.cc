//
// Copyright (C) 2015 The Android Open Source Project
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

#include "update_engine/payload_generator/verity_utils.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <chromeos/strings/string_utils.h>
extern "C" {
#include <vboot/vboot_host.h>
}

using std::string;
using std::vector;

extern "C" {

// vboot_host.h has a default VbExError() that will call exit() when a function
// fails. We redefine that function here so it doesn't exit.
void VbExError(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "ERROR: ");
  va_end(ap);
}

}

namespace {

// Splits a string with zero or more arguments separated by spaces into a list
// of strings, but respecting the double quotes. For example, the string:
//   a="foo" b=foo c="bar baz"   "my dir"/"my file"
// has only four arguments, since some parts are grouped together due to the
// double quotes.
vector<string> SplitQuotedArgs(const string arglist) {
  vector<string> terms = chromeos::string_utils::Split(
      arglist, " ", false, false);
  vector<string> result;
  string last_term;
  size_t quotes = 0;
  for (const string& term : terms) {
    if (quotes % 2 == 0 && term.empty())
      continue;

    quotes += std::count(term.begin(), term.end(), '"');
    if (last_term.empty()) {
      last_term = term;
    } else {
      last_term += " " + term;
    }
    if (quotes % 2 == 0) {
      result.push_back(last_term);
      last_term.clear();
      quotes = 0;
    }
  }
  // Unterminated quoted string found.
  if (!last_term.empty())
    result.push_back(last_term);
  return result;
}

}  // namespace

namespace chromeos_update_engine {

bool ParseVerityRootfsSize(const string& kernel_cmdline,
                           uint64_t* rootfs_size) {
  vector<string> kernel_args = SplitQuotedArgs(kernel_cmdline);

  for (const string& arg : kernel_args) {
    std::pair<string, string> key_value =
        chromeos::string_utils::SplitAtFirst(arg, "=", true);
    if (key_value.first != "dm")
      continue;
    string value = key_value.second;
    if (value.size() > 1 && value.front() == '"' && value.back() == '"')
      value = value.substr(1, value.size() - 1);

    vector<string> dm_parts = SplitQuotedArgs(value);
    // Check if this is a dm-verity device.
    if (std::find(dm_parts.begin(), dm_parts.end(), "verity") == dm_parts.end())
      continue;
    for (const string& dm_part : dm_parts) {
      key_value = chromeos::string_utils::SplitAtFirst(dm_part, "=", true);
      if (key_value.first != "hashstart")
        continue;
      if (!base::StringToUint64(key_value.second, rootfs_size))
        continue;
      // The hashstart= value is specified in 512-byte blocks, so we need to
      // convert that to bytes.
      *rootfs_size *= 512;
      return true;
    }
  }
  return false;
}

bool GetVerityRootfsSize(const string& kernel_dev, uint64_t* rootfs_size) {
  string kernel_cmdline;
  char *config = FindKernelConfig(kernel_dev.c_str(), USE_PREAMBLE_LOAD_ADDR);
  if (!config) {
    LOG(WARNING) << "Error retrieving kernel command line from '"
                 << kernel_dev << "', ignoring.";
    return false;
  }
  kernel_cmdline = string(config, MAX_KERNEL_CONFIG_SIZE);

  // FindKernelConfig() expects the caller to free the char*.
  free(config);

  if (!ParseVerityRootfsSize(kernel_cmdline, rootfs_size)) {
    LOG(INFO) << "Didn't find the rootfs size in the kernel command line: "
              << kernel_cmdline;
    return false;
  }
  return true;
}

}  // namespace chromeos_update_engine
