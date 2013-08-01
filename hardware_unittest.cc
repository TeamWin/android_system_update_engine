// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/test_utils.h"
#include "update_engine/hardware.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

class HardwareTest : public ::testing::Test {
 protected:
  void SetUp() {
  }
  void TearDown() {
  }

  // Hardware object under test.
  Hardware hwut_;
};

TEST_F(HardwareTest, BootDeviceTest) {
  // Pretty lame test...
  EXPECT_FALSE(hwut_.BootDevice().empty());
}

TEST_F(HardwareTest, KernelDeviceOfBootDevice) {
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("foo"));
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("/dev/sda0"));
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("/dev/sda1"));
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("/dev/sda2"));
  EXPECT_EQ("/dev/sda2", hwut_.KernelDeviceOfBootDevice("/dev/sda3"));
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("/dev/sda4"));
  EXPECT_EQ("/dev/sda4", hwut_.KernelDeviceOfBootDevice("/dev/sda5"));
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("/dev/sda6"));
  EXPECT_EQ("/dev/sda6", hwut_.KernelDeviceOfBootDevice("/dev/sda7"));
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("/dev/sda8"));
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("/dev/sda9"));

  EXPECT_EQ("/dev/mmcblk0p2", hwut_.KernelDeviceOfBootDevice("/dev/mmcblk0p3"));
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("/dev/mmcblk0p4"));

  EXPECT_EQ("/dev/ubi2", hwut_.KernelDeviceOfBootDevice("/dev/ubi3"));
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("/dev/ubi4"));

  EXPECT_EQ("/dev/mtdblock2",
            hwut_.KernelDeviceOfBootDevice("/dev/ubiblock3_0"));
  EXPECT_EQ("/dev/mtdblock4",
            hwut_.KernelDeviceOfBootDevice("/dev/ubiblock5_0"));
  EXPECT_EQ("/dev/mtdblock6",
            hwut_.KernelDeviceOfBootDevice("/dev/ubiblock7_0"));
  EXPECT_EQ("", hwut_.KernelDeviceOfBootDevice("/dev/ubiblock4_0"));
}

}  // namespace chromeos_update_engine
