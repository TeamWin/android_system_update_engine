#!/usr/bin/env python
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

"""Unit testing paycheck.py."""

# This test requires new (Y) and old (X) images, as well as a full payload
# from image Y and a delta payload from Y to X for each partition.
# Payloads are from sample_images/generate_payloads.
#
# The test performs the following:
#
# - It statically applies the full and delta payloads.
#
# - It applies full_payload to yield a new kernel (kern.part) and rootfs
#   (root.part) and compares them to the new image partitions.
#
# - It applies delta_payload to the old image to yield a new kernel and rootfs
#   and compares them to the new image partitions.
#
# Previously test_paycheck.sh. Run with update_payload ebuild.

# Disable check for function names to avoid errors based on old code
# pylint: disable=invalid-name

import filecmp
import os
import subprocess
import unittest


class PaycheckTest(unittest.TestCase):
  """Test paycheck functions."""

  def setUp(self):
    self.tmpdir = os.getenv('T')

    self._full_payload = os.path.join(self.tmpdir, 'full_payload.bin')
    self._delta_payload = os.path.join(self.tmpdir, 'delta_payload.bin')

    self._new_kernel = os.path.join(self.tmpdir, 'disk_ext2_4k.img')
    self._new_root = os.path.join(self.tmpdir, 'disk_sqfs_default.img')
    self._old_kernel = os.path.join(self.tmpdir,
                                    'disk_ext2_4k_empty.img')
    self._old_root = os.path.join(self.tmpdir, 'disk_sqfs_empty.img')

    # Temp output files.
    self._kernel_part = os.path.join(self.tmpdir, 'kern.part')
    self._root_part = os.path.join(self.tmpdir, 'root.part')

  def checkPayload(self, type_arg, payload):
    """Checks Payload."""
    self.assertEqual(0, subprocess.check_call(['./paycheck.py', '-t',
                                               type_arg, payload]))

  def testFullPayload(self):
    """Checks the full payload statically."""
    self.checkPayload('full', self._full_payload)

  def testDeltaPayload(self):
    """Checks the delta payload statically."""
    self.checkPayload('delta', self._delta_payload)

  def testApplyFullPayload(self):
    """Applies full payloads and compares results to new sample images."""
    self.assertEqual(0, subprocess.check_call(['./paycheck.py',
                                               self._full_payload,
                                               '--part_names', 'kernel', 'root',
                                               '--out_dst_part_paths',
                                               self._kernel_part,
                                               self._root_part]))

    # Check if generated full image is equal to sample image.
    self.assertTrue(filecmp.cmp(self._kernel_part, self._new_kernel))
    self.assertTrue(filecmp.cmp(self._root_part, self._new_root))

  def testApplyDeltaPayload(self):
    """Applies delta to old image and checks against new sample images."""
    self.assertEqual(0, subprocess.check_call(['./paycheck.py',
                                               self._delta_payload,
                                               '--part_names', 'kernel', 'root',
                                               '--src_part_paths',
                                               self._old_kernel, self._old_root,
                                               '--out_dst_part_paths',
                                               self._kernel_part,
                                               self._root_part]))

    self.assertTrue(filecmp.cmp(self._kernel_part, self._new_kernel))
    self.assertTrue(filecmp.cmp(self._root_part, self._new_root))

if __name__ == '__main__':
  unittest.main()
