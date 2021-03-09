#!/usr/bin/env python3
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

"""Command-line tool for converting OTA payloads to VABC style COW images."""

import os
import sys
import tempfile
import zipfile
import subprocess


def IsSparseImage(filepath):
  """Determine if an image is a sparse image
  Args:
    filepath: str, a path to an .img file

  Returns:
    return true iff the filepath is a sparse image.

  """
  with open(filepath, 'rb') as fp:
    # Magic for android sparse image format
    # https://source.android.com/devices/bootloader/images
    return fp.read(4) == b'\x3A\xFF\x26\xED'


def ConvertCOW(ota_path, target_file_path, tmp_dir, output_dir):
  """Convert ota payload to COW IMAGE
  Args:
    ota_path: str, path to ota.zip
    target_file_path: str, path to target_file.zip,
      must be the target build for OTA.
    tmp_dir: A temp dir as scratch space
    output_dir: A directory where all converted COW images will be written.
  """
  with zipfile.ZipFile(ota_path) as ota_zip:
    payload_path = ota_zip.extract("payload.bin", output_dir)
  with zipfile.ZipFile(target_file_path) as zfp:
    for fileinfo in zfp.infolist():
      img_name = os.path.basename(fileinfo.filename)
      if not fileinfo.filename.endswith(".img"):
        continue
      if fileinfo.filename.startswith("IMAGES/") or \
              fileinfo.filename.startswith("RADIO/"):
        img_path = zfp.extract(fileinfo, tmp_dir)
        target_img_path = os.path.join(output_dir, img_name)
        if IsSparseImage(img_path):
          subprocess.check_call(["simg2img", img_path, target_img_path])
        else:
          os.rename(img_path, target_img_path)
        print("Extracted", fileinfo.filename, "size:", fileinfo.file_size)

  subprocess.call(["cow_converter", payload_path,
                   output_dir])


def main():
  if len(sys.argv) != 4:
    print(
        "Usage:", sys.argv[0], "<your_ota.zip> <target_file.zip> <output dir>")
    return 1
  ota_path = sys.argv[1]
  target_file_path = sys.argv[2]
  output_dir = sys.argv[3]
  os.makedirs(output_dir, exist_ok=True)
  with tempfile.TemporaryDirectory() as tmp_dir:
    ConvertCOW(ota_path, target_file_path, tmp_dir, output_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
