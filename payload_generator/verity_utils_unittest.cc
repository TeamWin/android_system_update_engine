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

#include <gtest/gtest.h>

namespace chromeos_update_engine {

// A real kernel command line found on a device.
static const char* kVerityKernelCommandLine =
    "console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 "
    "root=/dev/dm-0 rootwait ro dm_verity.error_behavior=3 "
    "dm_verity.max_bios=-1 dm_verity.dev_wait=1 "
    "dm=\"1 vroot none ro 1,0 1536000 verity payload=PARTUUID=%U/PARTNROFF=1 "
    "hashtree=PARTUUID=%U/PARTNROFF=1 hashstart=1536000 alg=sha1 "
    "root_hexdigest=16b55bbea634fc3abf4c339da207cf050b1809d6 "
    "salt=18a095c4e473b68558afefdf83438d482cf37894d312afce6991c8267ea233f6\" "
    "noinitrd vt.global_cursor_default=0 kern_guid=%U ";

// A real kernel command line from a parrot device, including the bootcache.
static const char* kVerityAndBootcacheKernelCommandLine =
    "console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 "
    "root=/dev/dm-1 rootwait ro dm_verity.error_behavior=3 "
    "dm_verity.max_bios=-1 dm_verity.dev_wait=1 "
    "dm=\"2 vboot none ro 1,0 2545920 bootcache PARTUUID=%U/PARTNROFF=1 "
    "2545920 d5d03fb5459b6a75f069378c1799ba313d8ea89a 512 20000 100000, vroot "
    "none ro 1,0 2506752 verity payload=254:0 hashtree=254:0 hashstart=2506752 "
    "alg=sha1 root_hexdigest=3deebbc697a30cc585cf85a3b4351dc772861321 "
    "salt=6a13027cdf234c58a0b1f43e6a7428f41672cca89d5574c1f405649df65fb071\" "
    "noinitrd vt.global_cursor_default=0 kern_guid=%U add_efi_memmap "
    "boot=local noresume noswap i915.modeset=1 tpm_tis.force=1 "
    "tpm_tis.interrupts=0 nmi_watchdog=panic,lapic "
    "iTCO_vendor_support.vendorsupport=3";

TEST(VerityUtilsTest, ParseVerityRootfsSizeWithInvalidValues) {
  uint64_t rootfs_size = 0;
  EXPECT_FALSE(ParseVerityRootfsSize("", &rootfs_size));

  // Not a verity dm device.
  EXPECT_FALSE(ParseVerityRootfsSize(
      "dm=\"1 vroot none ro 1,0 1234 something\"", &rootfs_size));
  EXPECT_FALSE(ParseVerityRootfsSize(
      "ro verity hashattr=1234", &rootfs_size));

  // The verity doesn't have the hashstart= attribute.
  EXPECT_FALSE(ParseVerityRootfsSize(
      "dm=\"1 vroot none ro 1,0 1234 verity payload=fake\"", &rootfs_size));
}

TEST(VerityUtilsTest, ParseVerityRootfsSizeWithValidValues) {
  uint64_t rootfs_size = 0;
  EXPECT_TRUE(ParseVerityRootfsSize(kVerityKernelCommandLine, &rootfs_size));
  EXPECT_EQ(1536000 * 512, rootfs_size);
  EXPECT_TRUE(ParseVerityRootfsSize(kVerityAndBootcacheKernelCommandLine,
                                    &rootfs_size));
  EXPECT_EQ(2506752 * 512, rootfs_size);
}

}  // namespace chromeos_update_engine
