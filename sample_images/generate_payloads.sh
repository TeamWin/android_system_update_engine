#!/bin/bash
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

# This script generates some sample payloads from the images in
# sample_images.tar.bz2. and packages them in the sample_payloads.tar.xz file.
# The payloads are then used in paycheck_unittests.py. The file names
# must match the ones used in update_payload ebuild and paycheck_unittests.py.

set -e

TEMP_IMG_DIR=./sample_images
OLD_KERNEL="${TEMP_IMG_DIR}/disk_ext2_4k_empty.img"
OLD_ROOT="${TEMP_IMG_DIR}/disk_sqfs_empty.img"
NEW_KERNEL="${TEMP_IMG_DIR}/disk_ext2_4k.img"
NEW_ROOT="${TEMP_IMG_DIR}/disk_sqfs_default.img"


mkdir -p "${TEMP_IMG_DIR}"
tar -xvf sample_images.tar.bz2 -C "${TEMP_IMG_DIR}"

echo "Generating full payload"
delta_generator --out_file=full_payload.bin \
                --partition_names=kernel:root \
                --new_partitions="${NEW_KERNEL}":"${NEW_ROOT}"

echo "Generating delta payload"
delta_generator --out_file=delta_payload.bin \
                --partition_names=kernel:root \
                --new_partitions="${NEW_KERNEL}":"${NEW_ROOT}" \
                --old_partitions="${OLD_KERNEL}":"${OLD_ROOT}" --minor_version=6

echo "Creating sample_payloads.tar"
tar -cJf sample_payloads.tar.xz {delta,full}_payload.bin

rm -rf "${TEMP_IMG_DIR}" {delta,full}_payload.bin

echo "Done"
