#!/usr/bin/env python
#
# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""CLI script for linting .proto files inside update_engine."""

import sys
import re
import subprocess

def check_proto_file(commit_hash, filename):
  """Check if |filename| is consistnet with our protobuf guidelines

    Args:
      commit_hash: Hash of the git commit to look
      filename: A filesystem path to a .proto file
    Returns:
      True if this file passes linting check, False otherwise
  """
  output = subprocess.check_output(
      ["git", "diff", commit_hash+"~", commit_hash, "--", filename])
  output = output.decode()
  p = re.compile(r"^[+]?\s*required .*$", re.M)
  m = p.search(output)
  if m:
    print("File", filename,
          "contains 'required' keyword. Usage of required",
          "is strongly discouraged in protobuf", m.group())
    return False
  return True

def main():
  if len(sys.argv) < 2:
    print("Usage:", sys.argv[0], "commit_hash", "<file1>", "<file2>", "...")
    sys.exit(1)
  commit_hash = sys.argv[1]
  for filename in sys.argv[2:]:
    if filename.endswith(".proto"):
      if not check_proto_file(commit_hash, filename):
        sys.exit(1)

if __name__ == "__main__":
  main()
