#
# Copyright (C) 2020 The Android Open Source Project
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

"""Tools for running host side simulation of an OTA update."""


from __future__ import print_function

import argparse
import filecmp
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

import update_payload


def extract_file(zip_file_path, entry_name, target_file_path):
  """Extract a file from zip archive into |target_file_path|"""
  with open(target_file_path, 'wb') as out_fp:
    if isinstance(zip_file_path, zipfile.ZipFile):
      with zip_file_path.open(entry_name) as fp:
        shutil.copyfileobj(fp, out_fp)
    elif os.path.isdir(zip_file_path):
      with open(os.path.join(zip_file_path, entry_name), "rb") as fp:
        shutil.copyfileobj(fp, out_fp)

def is_sparse_image(filepath):
  with open(filepath, 'rb') as fp:
    # Magic for android sparse image format
    # https://source.android.com/devices/bootloader/images
    return fp.read(4) == b'\x3A\xFF\x26\xED'

def extract_img(zip_archive, img_name, output_path):
  entry_name = "IMAGES/" + img_name + ".img"
  extract_file(zip_archive, entry_name, output_path)
  if is_sparse_image(output_path):
    raw_img_path = output_path + ".raw"
    subprocess.check_output(["simg2img", output_path, raw_img_path])
    os.rename(raw_img_path, output_path)

def run_ota(source, target, payload_path, tempdir):
  """Run an OTA on host side"""
  payload = update_payload.Payload(payload_path)
  payload.Init()
  if zipfile.is_zipfile(source):
    source = zipfile.ZipFile(source)
  if zipfile.is_zipfile(target):
    target = zipfile.ZipFile(target)

  old_partitions = []
  new_partitions = []
  expected_new_partitions = []
  for part in payload.manifest.partitions:
    name = part.partition_name
    old_image = os.path.join(tempdir, "source_" + name + ".img")
    new_image = os.path.join(tempdir, "target_" + name + ".img")
    print("Extracting source image for", name)
    extract_img(source, name, old_image)
    print("Extracting target image for", name)
    extract_img(target, name, new_image)

    old_partitions.append(old_image)
    scratch_image_name = new_image + ".actual"
    new_partitions.append(scratch_image_name)
    with open(scratch_image_name, "wb") as fp:
      fp.truncate(part.new_partition_info.size)
    expected_new_partitions.append(new_image)

  delta_generator_args = ["delta_generator", "--in_file=" + payload_path]
  partition_names = [
      part.partition_name for part in payload.manifest.partitions
  ]
  delta_generator_args.append("--partition_names=" + ":".join(partition_names))
  delta_generator_args.append("--old_partitions=" + ":".join(old_partitions))
  delta_generator_args.append("--new_partitions=" + ":".join(new_partitions))

  subprocess.check_output(delta_generator_args)

  valid = True
  for (expected_part, actual_part, part_name) in \
      zip(expected_new_partitions, new_partitions, partition_names):
    if filecmp.cmp(expected_part, actual_part):
      print("Partition `{}` is valid".format(part_name))
    else:
      valid = False
      print(
          "Partition `{}` is INVALID expected image: {} actual image: {}"
          .format(part_name, expected_part, actual_part))

  if not valid and sys.stdout.isatty():
    input("Paused to investigate invalid partitions, press any key to exit.")


def main():
  parser = argparse.ArgumentParser(
      description="Run host side simulation of OTA package")
  parser.add_argument(
      "--source",
      help="Target file zip for the source build",
      required=True, nargs=1)
  parser.add_argument(
      "--target",
      help="Target file zip for the target build",
      required=True, nargs=1)
  parser.add_argument(
      "payload",
      help="payload.bin for the OTA package, or a zip of OTA package itself",
      nargs=1)
  args = parser.parse_args()
  print(args)

  assert os.path.exists(args.source[0]), \
    "source target file must point to a valid zipfile or directory"
  assert os.path.exists(args.target[0]), \
    "target path must point to a valid zipfile or directory"

  # pylint: disable=no-member
  with tempfile.TemporaryDirectory() as tempdir:
    payload_path = args.payload[0]
    if zipfile.is_zipfile(payload_path):
      with zipfile.ZipFile(payload_path, "r") as zfp:
        payload_entry_name = 'payload.bin'
        zfp.extract(payload_entry_name, tempdir)
        payload_path = os.path.join(tempdir, payload_entry_name)

    run_ota(args.source[0], args.target[0], payload_path, tempdir)


if __name__ == '__main__':
  main()
