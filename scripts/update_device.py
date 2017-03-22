#!/usr/bin/env python
#
# Copyright (C) 2017 The Android Open Source Project
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

"""Send an A/B update to an Android device over adb."""

import argparse
import BaseHTTPServer
import logging
import os
import socket
import subprocess
import sys
import threading
import zipfile


# The path used to store the OTA package when applying the package from a file.
OTA_PACKAGE_PATH = '/data/ota_package'


def CopyFileObjLength(fsrc, fdst, buffer_size=128 * 1024, copy_length=None):
  """Copy from a file object to another.

  This function is similar to shutil.copyfileobj except that it allows to copy
  less than the full source file.

  Args:
    fsrc: source file object where to read from.
    fdst: destination file object where to write to.
    buffer_size: size of the copy buffer in memory.
    copy_length: maximum number of bytes to copy, or None to copy everything.

  Returns:
    the number of bytes copied.
  """
  copied = 0
  while True:
    chunk_size = buffer_size
    if copy_length is not None:
      chunk_size = min(chunk_size, copy_length - copied)
      if not chunk_size:
        break
    buf = fsrc.read(chunk_size)
    if not buf:
      break
    fdst.write(buf)
    copied += len(buf)
  return copied


class AndroidOTAPackage(object):
  """Android update payload using the .zip format.

  Android OTA packages traditionally used a .zip file to store the payload. When
  applying A/B updates over the network, a payload binary is stored RAW inside
  this .zip file which is used by update_engine to apply the payload. To do
  this, an offset and size inside the .zip file are provided.
  """

  # Android OTA package file paths.
  OTA_PAYLOAD_BIN = 'payload.bin'
  OTA_PAYLOAD_PROPERTIES_TXT = 'payload_properties.txt'

  def __init__(self, otafilename):
    self.otafilename = otafilename

    otazip = zipfile.ZipFile(otafilename, 'r')
    payload_info = otazip.getinfo(self.OTA_PAYLOAD_BIN)
    self.offset = payload_info.header_offset + len(payload_info.FileHeader())
    self.size = payload_info.file_size
    self.properties = otazip.read(self.OTA_PAYLOAD_PROPERTIES_TXT)


class UpdateHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """A HTTPServer that supports single-range requests.

  Attributes:
    serving_payload: path to the only payload file we are serving.
  """

  @staticmethod
  def _ParseRange(range_str, file_size):
    """Parse an HTTP range string.

    Args:
      range_str: HTTP Range header in the request, not including "Header:".
      file_size: total size of the serving file.

    Returns:
      A tuple (start_range, end_range) with the range of bytes requested.
    """
    start_range = 0
    end_range = file_size

    if range_str:
      range_str = range_str.split('=', 1)[1]
      s, e = range_str.split('-', 1)
      if s:
        start_range = int(s)
        if e:
          end_range = int(e) + 1
      elif e:
        if int(e) < file_size:
          start_range = file_size - int(e)
    return start_range, end_range


  def do_GET(self):  # pylint: disable=invalid-name
    """Reply with the requested payload file."""
    if self.path != '/payload':
      self.send_error(404, 'Unknown request')
      return

    if not self.serving_payload:
      self.send_error(500, 'No serving payload set')
      return

    try:
      f = open(self.serving_payload, 'rb')
    except IOError:
      self.send_error(404, 'File not found')
      return
    # Handle the range request.
    if 'Range' in self.headers:
      self.send_response(206)
    else:
      self.send_response(200)

    stat = os.fstat(f.fileno())
    start_range, end_range = self._ParseRange(self.headers.get('range'),
                                              stat.st_size)
    logging.info('Serving request for %s from %s [%d, %d) length: %d',
             self.path, self.serving_payload, start_range, end_range,
             end_range - start_range)

    self.send_header('Accept-Ranges', 'bytes')
    self.send_header('Content-Range',
                     'bytes ' + str(start_range) + '-' + str(end_range - 1) +
                     '/' + str(end_range - start_range))
    self.send_header('Content-Length', end_range - start_range)

    self.send_header('Last-Modified', self.date_time_string(stat.st_mtime))
    self.send_header('Content-type', 'application/octet-stream')
    self.end_headers()

    f.seek(start_range)
    CopyFileObjLength(f, self.wfile, copy_length=end_range - start_range)


class ServerThread(threading.Thread):
  """A thread for serving HTTP requests."""

  def __init__(self, ota_filename):
    threading.Thread.__init__(self)
    # serving_payload is a class attribute and the UpdateHandler class is
    # instantiated with every request.
    UpdateHandler.serving_payload = ota_filename
    self._httpd = BaseHTTPServer.HTTPServer(('127.0.0.1', 0), UpdateHandler)
    self.port = self._httpd.server_port

  def run(self):
    try:
      self._httpd.serve_forever()
    except (KeyboardInterrupt, socket.error):
      pass
    logging.info('Server Terminated')

  def StopServer(self):
    self._httpd.socket.close()


def StartServer(ota_filename):
  t = ServerThread(ota_filename)
  t.start()
  return t


def AndroidUpdateCommand(ota_filename, payload_url):
  """Return the command to run to start the update in the Android device."""
  ota = AndroidOTAPackage(ota_filename)
  headers = ota.properties
  headers += 'USER_AGENT=Dalvik (something, something)\n'

  # headers += 'POWERWASH=1\n'
  headers += 'NETWORK_ID=0\n'

  return ['update_engine_client', '--update', '--follow',
          '--payload=%s' % payload_url, '--offset=%d' % ota.offset,
          '--size=%d' % ota.size, '--headers="%s"' % headers]


class AdbHost(object):
  """Represents a device connected via ADB."""

  def __init__(self, device_serial=None):
    """Construct an instance.

    Args:
        device_serial: options string serial number of attached device.
    """
    self._device_serial = device_serial
    self._command_prefix = ['adb']
    if self._device_serial:
      self._command_prefix += ['-s', self._device_serial]

  def adb(self, command):
    """Run an ADB command like "adb push".

    Args:
      command: list of strings containing command and arguments to run

    Returns:
      the program's return code.

    Raises:
      subprocess.CalledProcessError on command exit != 0.
    """
    command = self._command_prefix + command
    logging.info('Running: %s', ' '.join(str(x) for x in command))
    p = subprocess.Popen(command, universal_newlines=True)
    p.wait()
    return p.returncode


def main():
  parser = argparse.ArgumentParser(description='Android A/B OTA helper.')
  parser.add_argument('otafile', metavar='ZIP', type=str,
                      help='the OTA package file (a .zip file).')
  parser.add_argument('--file', action='store_true',
                      help='Push the file to the device before updating.')
  parser.add_argument('--no-push', action='store_true',
                      help='Skip the "push" command when using --file')
  parser.add_argument('-s', type=str, default='', metavar='DEVICE',
                      help='The specific device to use.')
  parser.add_argument('--no-verbose', action='store_true',
                      help='Less verbose output')
  args = parser.parse_args()
  logging.basicConfig(
      level=logging.WARNING if args.no_verbose else logging.INFO)

  dut = AdbHost(args.s)

  server_thread = None
  # List of commands to execute on exit.
  finalize_cmds = []
  # Commands to execute when canceling an update.
  cancel_cmd = ['shell', 'su', '0', 'update_engine_client', '--cancel']
  # List of commands to perform the update.
  cmds = []

  if args.file:
    # Update via pushing a file to /data.
    device_ota_file = os.path.join(OTA_PACKAGE_PATH, 'debug.zip')
    payload_url = 'file://' + device_ota_file
    if not args.no_push:
      cmds.append(['push', args.otafile, device_ota_file])
    cmds.append(['shell', 'su', '0', 'chown', 'system:cache', device_ota_file])
    cmds.append(['shell', 'su', '0', 'chmod', '0660', device_ota_file])
  else:
    # Update via sending the payload over the network with an "adb reverse"
    # command.
    device_port = 1234
    payload_url = 'http://127.0.0.1:%d/payload' % device_port
    server_thread = StartServer(args.otafile)
    cmds.append(
        ['reverse', 'tcp:%d' % device_port, 'tcp:%d' % server_thread.port])
    finalize_cmds.append(['reverse', '--remove', 'tcp:%d' % device_port])

  try:
    # The main update command using the configured payload_url.
    update_cmd = AndroidUpdateCommand(args.otafile, payload_url)
    cmds.append(['shell', 'su', '0'] + update_cmd)

    for cmd in cmds:
      dut.adb(cmd)
  except KeyboardInterrupt:
    dut.adb(cancel_cmd)
  finally:
    if server_thread:
      server_thread.StopServer()
    for cmd in finalize_cmds:
      dut.adb(cmd)

  return 0

if __name__ == '__main__':
  sys.exit(main())
