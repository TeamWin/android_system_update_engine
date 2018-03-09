#!/usr/bin/python2
#
# Copyright (C) 2013 The Android Open Source Project
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

"""Command-line tool for checking and applying Chrome OS update payloads."""

from __future__ import print_function

# pylint: disable=import-error
import argparse
import filecmp
import os
import sys
import tempfile

from update_payload import error

lib_dir = os.path.join(os.path.dirname(__file__), 'lib')
if os.path.exists(lib_dir) and os.path.isdir(lib_dir):
  sys.path.insert(1, lib_dir)
import update_payload


_TYPE_FULL = 'full'
_TYPE_DELTA = 'delta'


def ParseArguments(argv):
  """Parse and validate command-line arguments.

  Args:
    argv: command-line arguments to parse (excluding the program name)

  Returns:
    Returns the arguments returned by the argument parser.
  """
  parser = argparse.ArgumentParser(
      description=('Applies a Chrome OS update PAYLOAD to src_kern and '
                   'src_root emitting dst_kern and dst_root, respectively. '
                   'src_kern and src_root are only needed for delta payloads. '
                   'When no partitions are provided, verifies the payload '
                   'integrity.'),
      epilog=('Note: a payload may verify correctly but fail to apply, and '
              'vice versa; this is by design and can be thought of as static '
              'vs dynamic correctness. A payload that both verifies and '
              'applies correctly should be safe for use by the Chrome OS '
              'Update Engine. Use --check to verify a payload prior to '
              'applying it.'),
      formatter_class=argparse.RawDescriptionHelpFormatter
  )

  check_args = parser.add_argument_group('Checking payload integrity')
  check_args.add_argument('-c', '--check', action='store_true', default=False,
                          help=('force payload integrity check (e.g. before '
                                'applying)'))
  check_args.add_argument('-D', '--describe', action='store_true',
                          default=False,
                          help='Print a friendly description of the payload.')
  check_args.add_argument('-r', '--report', metavar='FILE',
                          help="dump payload report (`-' for stdout)")
  check_args.add_argument('-t', '--type', dest='assert_type',
                          help='assert the payload type',
                          choices=[_TYPE_FULL, _TYPE_DELTA])
  check_args.add_argument('-z', '--block-size', metavar='NUM', default=0,
                          type=int,
                          help='assert a non-default (4096) payload block size')
  check_args.add_argument('-u', '--allow-unhashed', action='store_true',
                          default=False, help='allow unhashed operations')
  check_args.add_argument('-d', '--disabled_tests', default=(), metavar='',
                          help=('space separated list of tests to disable. '
                                'allowed options include: ' +
                                ', '.join(update_payload.CHECKS_TO_DISABLE)),
                          choices=update_payload.CHECKS_TO_DISABLE)
  check_args.add_argument('-k', '--key', metavar='FILE',
                          help=('override standard key used for signature '
                                'validation'))
  check_args.add_argument('-m', '--meta-sig', metavar='FILE',
                          help='verify metadata against its signature')
  check_args.add_argument('-p', '--root-part-size', metavar='NUM',
                          default=0, type=int,
                          help='override rootfs partition size auto-inference')
  check_args.add_argument('-P', '--kern-part-size', metavar='NUM',
                          default=0, type=int,
                          help='override kernel partition size auto-inference')

  apply_args = parser.add_argument_group('Applying payload')
  # TODO(ahassani): Extent extract-bsdiff to puffdiff too.
  apply_args.add_argument('-x', '--extract-bsdiff', action='store_true',
                          default=False,
                          help=('use temp input/output files with BSDIFF '
                                'operations (not in-place)'))
  apply_args.add_argument('--bspatch-path', metavar='FILE',
                          help='use the specified bspatch binary')
  apply_args.add_argument('--puffpatch-path', metavar='FILE',
                          help='use the specified puffpatch binary')
  apply_args.add_argument('--dst_kern', metavar='FILE',
                          help='destination kernel partition file')
  apply_args.add_argument('--dst_root', metavar='FILE',
                          help='destination root partition file')
  apply_args.add_argument('--src_kern', metavar='FILE',
                          help='source kernel partition file')
  apply_args.add_argument('--src_root', metavar='FILE',
                          help='source root partition file')
  apply_args.add_argument('--out_dst_kern', metavar='FILE',
                          help='created destination kernel partition file')
  apply_args.add_argument('--out_dst_root', metavar='FILE',
                          help='created destination root partition file')

  parser.add_argument('payload', metavar='PAYLOAD', help='the payload file')

  # Parse command-line arguments.
  args = parser.parse_args(argv)

  # There are several options that imply --check.
  args.check = (args.check or args.report or args.assert_type or
                args.block_size or args.allow_unhashed or
                args.disabled_tests or args.meta_sig or args.key or
                args.root_part_size or args.kern_part_size)

  # Check the arguments, enforce payload type accordingly.
  if (args.src_kern is None) != (args.src_root is None):
    parser.error('--src_kern and --src_root should be given together')
  if (args.dst_kern is None) != (args.dst_root is None):
    parser.error('--dst_kern and --dst_root should be given together')
  if (args.out_dst_kern is None) != (args.out_dst_root is None):
    parser.error('--out_dst_kern and --out_dst_root should be given together')

  if (args.dst_kern and args.dst_root) or \
     (args.out_dst_kern and args.out_dst_root):
    if args.src_kern and args.src_root:
      if args.assert_type == _TYPE_FULL:
        parser.error('%s payload does not accept source partition arguments'
                     % _TYPE_FULL)
      else:
        args.assert_type = _TYPE_DELTA
    else:
      if args.assert_type == _TYPE_DELTA:
        parser.error('%s payload requires source partitions arguments'
                     % _TYPE_DELTA)
      else:
        args.assert_type = _TYPE_FULL
  else:
    # Not applying payload.
    if args.extract_bsdiff:
      parser.error('--extract-bsdiff can only be used when applying payloads')
    if args.bspatch_path:
      parser.error('--bspatch-path can only be used when applying payloads')
    if args.puffpatch_path:
      parser.error('--puffpatch-path can only be used when applying payloads')

  # By default, look for a metadata-signature file with a name based on the name
  # of the payload we are checking. We only do it if check was triggered.
  if args.check and not args.meta_sig:
    default_meta_sig = args.payload + '.metadata-signature'
    if os.path.isfile(default_meta_sig):
      args.meta_sig = default_meta_sig
      print('Using default metadata signature', args.meta_sig, file=sys.stderr)

  return args


def main(argv):
  # Parse and validate arguments.
  args = ParseArguments(argv[1:])

  with open(args.payload) as payload_file:
    payload = update_payload.Payload(payload_file)
    try:
      # Initialize payload.
      payload.Init()

      if args.describe:
        payload.Describe()

      # Perform payload integrity checks.
      if args.check:
        report_file = None
        do_close_report_file = False
        metadata_sig_file = None
        try:
          if args.report:
            if args.report == '-':
              report_file = sys.stdout
            else:
              report_file = open(args.report, 'w')
              do_close_report_file = True

          metadata_sig_file = args.meta_sig and open(args.meta_sig)
          payload.Check(
              pubkey_file_name=args.key,
              metadata_sig_file=metadata_sig_file,
              report_out_file=report_file,
              assert_type=args.assert_type,
              block_size=int(args.block_size),
              rootfs_part_size=args.root_part_size,
              kernel_part_size=args.kern_part_size,
              allow_unhashed=args.allow_unhashed,
              disabled_tests=args.disabled_tests)
        finally:
          if metadata_sig_file:
            metadata_sig_file.close()
          if do_close_report_file:
            report_file.close()

      # Apply payload.
      if (args.dst_root and args.dst_kern) or \
         (args.out_dst_root and args.out_dst_kern):
        dargs = {'bsdiff_in_place': not args.extract_bsdiff}
        if args.bspatch_path:
          dargs['bspatch_path'] = args.bspatch_path
        if args.puffpatch_path:
          dargs['puffpatch_path'] = args.puffpatch_path
        if args.assert_type == _TYPE_DELTA:
          dargs['old_kernel_part'] = args.src_kern
          dargs['old_rootfs_part'] = args.src_root

        if args.out_dst_kern and args.out_dst_root:
          out_dst_kern = open(args.out_dst_kern, 'w+')
          out_dst_root = open(args.out_dst_root, 'w+')
        else:
          out_dst_kern = tempfile.NamedTemporaryFile()
          out_dst_root = tempfile.NamedTemporaryFile()

        payload.Apply(out_dst_kern.name, out_dst_root.name, **dargs)

        # If destination kernel and rootfs partitions are not given, then this
        # just becomes an apply operation with no check.
        if args.dst_kern and args.dst_root:
          # Prior to comparing, add the unused space past the filesystem
          # boundary in the new target partitions to become the same size as
          # the given partitions. This will truncate to larger size.
          out_dst_kern.truncate(os.path.getsize(args.dst_kern))
          out_dst_root.truncate(os.path.getsize(args.dst_root))

          # Compare resulting partitions with the ones from the target image.
          if not filecmp.cmp(out_dst_kern.name, args.dst_kern):
            raise error.PayloadError('Resulting kernel partition corrupted.')
          if not filecmp.cmp(out_dst_root.name, args.dst_root):
            raise error.PayloadError('Resulting rootfs partition corrupted.')

        # Close the output files. If args.out_dst_* was not given, then these
        # files are created as temp files and will be deleted upon close().
        out_dst_kern.close()
        out_dst_root.close()

    except error.PayloadError, e:
      sys.stderr.write('Error: %s\n' % e)
      return 1

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
