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

"""Repeatedly install an A/B update to an Android device over adb."""

import argparse
import sys
from pathlib import Path
import subprocess
import signal


def CleanupLoopDevices():
  # b/184716804 clean up unused loop devices
  subprocess.check_call(["adb", "shell", "su", "0", "losetup", '-D'])


def CancelOTA():
  subprocess.call(["adb", "shell", "su", "0",
                  "update_engine_client", "--cancel"])


def PerformOTAThenPause(otafile: Path, update_device_script: Path):
  python = sys.executable
  ota_cmd = [python, str(update_device_script), str(otafile),
             "--no-postinstall", "--no-slot-switch"]
  p = subprocess.Popen(ota_cmd)
  pid = p.pid
  try:
    ret = p.wait(10)
    if ret is not None and ret != 0:
      raise RuntimeError("OTA failed to apply")
    if ret == 0:
      print("OTA finished early? Surprise.")
      return
  except subprocess.TimeoutExpired:
    pass
  print(f"Killing {pid}")
  subprocess.check_call(["pkill", "-INT", "-P", str(pid)])
  p.send_signal(signal.SIGINT)
  p.wait()


def PerformTest(otafile: Path, resumes: int, timeout: int):
  """Install an OTA to device, raising exceptions on failure

  Args:
    otafile: Path to the ota.zip to install

  Return:
    None if no error, if there's an error exception will be thrown
  """
  assert otafile.exists()
  print("Applying", otafile)
  script_dir = Path(__file__).parent.absolute()
  update_device_script = script_dir / "update_device.py"
  assert update_device_script.exists()
  print(update_device_script)
  python = sys.executable

  for i in range(resumes):
    print("Pause/Resume for the", i+1, "th time")
    PerformOTAThenPause(otafile, update_device_script)
  CancelOTA()
  CleanupLoopDevices()

  ota_cmd = [python, str(update_device_script),
             str(otafile), "--no-postinstall"]
  print("Finishing OTA Update", ota_cmd)
  output = subprocess.check_output(
      ota_cmd, stderr=subprocess.STDOUT, timeout=timeout).decode()
  print(output)
  if "onPayloadApplicationComplete(ErrorCode::kSuccess" not in output:
    raise RuntimeError("Failed to finish OTA")
  subprocess.call(
      ["adb", "shell", "su", "0", "update_engine_client", "--cancel"])
  subprocess.check_call(
      ["adb", "shell", "su", "0", "update_engine_client", "--reset_status"])
  CleanupLoopDevices()


def main():
  parser = argparse.ArgumentParser(
      description='Android A/B OTA stress test helper.')
  parser.add_argument('otafile', metavar='PAYLOAD', type=Path,
                      help='the OTA package file (a .zip file) or raw payload \
                      if device uses Omaha.')
  parser.add_argument('-n', "--iterations", type=int, default=10,
                      metavar='ITERATIONS',
                      help='The number of iterations to run the stress test, or\
                       -1 to keep running until CTRL+C')
  parser.add_argument('-r', "--resumes", type=int, default=5, metavar='RESUMES',
                      help='The number of iterations to pause the update when \
                        installing')
  parser.add_argument('-t', "--timeout", type=int, default=60*60,
                      metavar='TIMEOUTS',
                      help='Timeout, in seconds, when waiting for OTA to \
                        finish')
  args = parser.parse_args()
  print(args)
  n = args.iterations
  while n != 0:
    PerformTest(args.otafile, args.resumes, args.timeout)
    n -= 1


if __name__ == "__main__":
  main()
