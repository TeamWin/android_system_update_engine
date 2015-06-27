#!/bin/bash

# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# cleanup <path>
# Unmount and remove the mountpoint <path>
cleanup() {
  if ! sudo umount "$1" 2>/dev/null; then
    if mountpoint -q "$1"; then
      sync && sudo umount "$1"
    fi
  fi
  rmdir "$1"
}

# add_files_default <mntdir> <block_size>
# Add several test files to the image mounted in <mntdir>.
add_files_default() {
  local mntdir="$1"
  local block_size="$2"

  ### Generate the files used in unittest with descriptive names.
  sudo touch "${mntdir}"/empty-file

  # regular: Regular files.
  echo "small file" | sudo dd of="${mntdir}"/regular-small status=none
  dd if=/dev/zero bs=1024 count=16 status=none | tr '\0' '\141' |
    sudo dd of="${mntdir}"/regular-16k status=none
  sudo dd if=/dev/zero of="${mntdir}"/regular-32k-zeros bs=1024 count=16 \
    status=none

  echo "with net_cap" | sudo dd of="${mntdir}"/regular-with_net_cap status=none
  sudo setcap cap_net_raw=ep "${mntdir}"/regular-with_net_cap

  # sparse_empty: Files with no data blocks at all (only sparse holes).
  sudo truncate --size=10240 "${mntdir}"/sparse_empty-10k
  sudo truncate --size=$(( block_size * 2 )) "${mntdir}"/sparse_empty-2blocks

  # sparse: Files with some data blocks but also sparse holes.
  echo -n "foo" |
    sudo dd of="${mntdir}"/sparse-16k-last_block bs=1 \
      seek=$(( 16 * 1024 - 3)) status=none

  # ext2 inodes have 12 direct blocks, one indirect, one double indirect and
  # one triple indirect. 10000 should be enough to have an indirect and double
  # indirect block.
  echo -n "foo" |
    sudo dd of="${mntdir}"/sparse-10000blocks bs=1 \
      seek=$(( block_size * 10000 )) status=none

  sudo truncate --size=16384 "${mntdir}"/sparse-16k-first_block
  echo "first block" | sudo dd of="${mntdir}"/sparse-16k-first_block status=none

  sudo truncate --size=16384 "${mntdir}"/sparse-16k-holes
  echo "a" | sudo dd of="${mntdir}"/sparse-16k-holes bs=1 seek=100 status=none
  echo "b" | sudo dd of="${mntdir}"/sparse-16k-holes bs=1 seek=10000 status=none

  # link: symlinks and hardlinks.
  sudo ln -s "broken-link" "${mntdir}"/link-short_symlink
  sudo ln -s $(dd if=/dev/zero bs=256 count=1 status=none | tr '\0' '\141') \
    "${mntdir}"/link-long_symlink
  sudo ln "${mntdir}"/regular-16k "${mntdir}"/link-hard-regular-16k

  # Directories.
  sudo mkdir -p "${mntdir}"/dir1/dir2/dir1
  echo "foo" | sudo tee "${mntdir}"/dir1/dir2/file >/dev/null
  echo "bar" | sudo tee "${mntdir}"/dir1/file >/dev/null

  # removed: removed files that should not be listed.
  echo "We will remove this file so it's contents will be somewhere in the " \
    "empty space data but it won't be all zeros." |
    sudo dd of="${mntdir}"/removed conv=fsync status=none
  sudo rm "${mntdir}"/removed
}

# add_files_ue_settings <mntdir> <block_size>
# Add the update_engine.conf settings file. This file contains the
add_files_ue_settings() {
  local mntdir="$1"

  sudo mkdir -p "${mntdir}"/etc >/dev/null
  sudo tee "${mntdir}"/etc/update_engine.conf >/dev/null <<EOF
PAYLOAD_MINOR_VERSION=1234
EOF
  # Example of a real lsb-release file released on link stable.
  sudo tee "${mntdir}"/etc/lsb-release >/dev/null <<EOF
CHROMEOS_AUSERVER=https://tools.google.com/service/update2
CHROMEOS_BOARD_APPID={F26D159B-52A3-491A-AE25-B23670A66B32}
CHROMEOS_CANARY_APPID={90F229CE-83E2-4FAF-8479-E368A34938B1}
CHROMEOS_DEVSERVER=
CHROMEOS_RELEASE_APPID={F26D159B-52A3-491A-AE25-B23670A66B32}
CHROMEOS_RELEASE_BOARD=link-signed-mp-v4keys
CHROMEOS_RELEASE_BRANCH_NUMBER=63
CHROMEOS_RELEASE_BUILD_NUMBER=6946
CHROMEOS_RELEASE_BUILD_TYPE=Official Build
CHROMEOS_RELEASE_CHROME_MILESTONE=43
CHROMEOS_RELEASE_DESCRIPTION=6946.63.0 (Official Build) stable-channel link
CHROMEOS_RELEASE_NAME=Chrome OS
CHROMEOS_RELEASE_PATCH_NUMBER=0
CHROMEOS_RELEASE_TRACK=stable-channel
CHROMEOS_RELEASE_VERSION=6946.63.0
GOOGLE_RELEASE=6946.63.0
EOF
}

# generate_fs <filename> <kind> <size> [block_size] [block_groups]
generate_fs() {
  local filename="$1"
  local kind="$2"
  local size="$3"
  local block_size="${4:-4096}"
  local block_groups="${5:-}"

  local mkfs_opts=( -q -F -b "${block_size}" -L "ROOT-TEST" -t ext2 )
  if [[ -n "${block_groups}" ]]; then
    mkfs_opts+=( -G "${block_groups}" )
  fi

  local mntdir=$(mktemp --tmpdir -d generate_ext2.XXXXXX)
  trap 'cleanup "${mntdir}"; rm -f "${filename}"' INT TERM EXIT

  # Cleanup old image.
  if [[ -e "${filename}" ]]; then
    rm -f "${filename}"
  fi
  truncate --size="${size}" "${filename}"

  mkfs.ext2 "${mkfs_opts[@]}" "${filename}"
  sudo mount "${filename}" "${mntdir}" -o loop

  case "${kind}" in
    ue_settings)
      add_files_ue_settings "${mntdir}" "${block_size}"
      ;;
    default)
      add_files_default "${mntdir}" "${block_size}"
      ;;
  esac

  cleanup "${mntdir}"
  trap - INT TERM EXIT
}

image_desc="${1:-}"
output_dir="${2:-}"

if [[ ! -e "${image_desc}" || ! -d "${output_dir}" ]]; then
  echo "Use: $0 <image_description.txt> <output_dir>" >&2
  exit 1
fi

args=( $(cat ${image_desc}) )
dest_image="${output_dir}/$(basename ${image_desc} .txt).img"
generate_fs "${dest_image}" "${args[@]}"
