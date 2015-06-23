// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/utils.h"

#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <map>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>

#include "update_engine/fake_clock.h"
#include "update_engine/fake_prefs.h"
#include "update_engine/fake_system_state.h"
#include "update_engine/prefs.h"
#include "update_engine/test_utils.h"

using std::map;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class UtilsTest : public ::testing::Test { };

TEST(UtilsTest, CanParseECVersion) {
  // Should be able to parse and valid key value line.
  EXPECT_EQ("12345", utils::ParseECVersion("fw_version=12345"));
  EXPECT_EQ("123456", utils::ParseECVersion(
      "b=1231a fw_version=123456 a=fasd2"));
  EXPECT_EQ("12345", utils::ParseECVersion("fw_version=12345"));
  EXPECT_EQ("00VFA616", utils::ParseECVersion(
      "vendor=\"sam\" fw_version=\"00VFA616\""));

  // For invalid entries, should return the empty string.
  EXPECT_EQ("", utils::ParseECVersion("b=1231a fw_version a=fasd2"));
}


TEST(UtilsTest, KernelDeviceOfBootDevice) {
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice(""));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("foo"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/sda0"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/sda1"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/sda2"));
  EXPECT_EQ("/dev/sda2", utils::KernelDeviceOfBootDevice("/dev/sda3"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/sda4"));
  EXPECT_EQ("/dev/sda4", utils::KernelDeviceOfBootDevice("/dev/sda5"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/sda6"));
  EXPECT_EQ("/dev/sda6", utils::KernelDeviceOfBootDevice("/dev/sda7"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/sda8"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/sda9"));

  EXPECT_EQ("/dev/mmcblk0p2",
            utils::KernelDeviceOfBootDevice("/dev/mmcblk0p3"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/mmcblk0p4"));

  EXPECT_EQ("/dev/mtd2", utils::KernelDeviceOfBootDevice("/dev/ubi3"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/ubi4"));

  EXPECT_EQ("/dev/mtd2",
            utils::KernelDeviceOfBootDevice("/dev/ubiblock3_0"));
  EXPECT_EQ("/dev/mtd4",
            utils::KernelDeviceOfBootDevice("/dev/ubiblock5_0"));
  EXPECT_EQ("/dev/mtd6",
            utils::KernelDeviceOfBootDevice("/dev/ubiblock7_0"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/ubiblock4_0"));
}

TEST(UtilsTest, ReadFileFailure) {
  chromeos::Blob empty;
  EXPECT_FALSE(utils::ReadFile("/this/doesn't/exist", &empty));
}

TEST(UtilsTest, ReadFileChunk) {
  base::FilePath file;
  EXPECT_TRUE(base::CreateTemporaryFile(&file));
  ScopedPathUnlinker unlinker(file.value());
  chromeos::Blob data;
  const size_t kSize = 1024 * 1024;
  for (size_t i = 0; i < kSize; i++) {
    data.push_back(i % 255);
  }
  EXPECT_TRUE(utils::WriteFile(file.value().c_str(), data.data(), data.size()));
  chromeos::Blob in_data;
  EXPECT_TRUE(utils::ReadFileChunk(file.value().c_str(), kSize, 10, &in_data));
  EXPECT_TRUE(in_data.empty());
  EXPECT_TRUE(utils::ReadFileChunk(file.value().c_str(), 0, -1, &in_data));
  EXPECT_TRUE(data == in_data);
  in_data.clear();
  EXPECT_TRUE(utils::ReadFileChunk(file.value().c_str(), 10, 20, &in_data));
  EXPECT_TRUE(chromeos::Blob(data.begin() + 10, data.begin() + 10 + 20) ==
              in_data);
}

TEST(UtilsTest, ErrnoNumberAsStringTest) {
  EXPECT_EQ("No such file or directory", utils::ErrnoNumberAsString(ENOENT));
}

TEST(UtilsTest, IsSymlinkTest) {
  string temp_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("symlink-test.XXXXXX", &temp_dir));
  string temp_file = temp_dir + "/temp-file";
  EXPECT_TRUE(utils::WriteFile(temp_file.c_str(), "", 0));
  string temp_symlink = temp_dir + "/temp-symlink";
  EXPECT_EQ(0, symlink(temp_file.c_str(), temp_symlink.c_str()));
  EXPECT_FALSE(utils::IsSymlink(temp_dir.c_str()));
  EXPECT_FALSE(utils::IsSymlink(temp_file.c_str()));
  EXPECT_TRUE(utils::IsSymlink(temp_symlink.c_str()));
  EXPECT_FALSE(utils::IsSymlink("/non/existent/path"));
  EXPECT_TRUE(test_utils::RecursiveUnlinkDir(temp_dir));
}

TEST(UtilsTest, GetDiskNameTest) {
  EXPECT_EQ("/dev/sda", utils::GetDiskName("/dev/sda3"));
  EXPECT_EQ("/dev/sdp", utils::GetDiskName("/dev/sdp1234"));
  EXPECT_EQ("/dev/mmcblk0", utils::GetDiskName("/dev/mmcblk0p3"));
  EXPECT_EQ("", utils::GetDiskName("/dev/mmcblk0p"));
  EXPECT_EQ("", utils::GetDiskName("/dev/sda"));
  EXPECT_EQ("/dev/ubiblock", utils::GetDiskName("/dev/ubiblock3_2"));
  EXPECT_EQ("", utils::GetDiskName("/dev/foo/bar"));
  EXPECT_EQ("", utils::GetDiskName("/"));
  EXPECT_EQ("", utils::GetDiskName(""));
}

TEST(UtilsTest, SysfsBlockDeviceTest) {
  EXPECT_EQ("/sys/block/sda", utils::SysfsBlockDevice("/dev/sda"));
  EXPECT_EQ("", utils::SysfsBlockDevice("/foo/sda"));
  EXPECT_EQ("", utils::SysfsBlockDevice("/dev/foo/bar"));
  EXPECT_EQ("", utils::SysfsBlockDevice("/"));
  EXPECT_EQ("", utils::SysfsBlockDevice("./"));
  EXPECT_EQ("", utils::SysfsBlockDevice(""));
}

TEST(UtilsTest, IsRemovableDeviceTest) {
  EXPECT_FALSE(utils::IsRemovableDevice(""));
  EXPECT_FALSE(utils::IsRemovableDevice("/dev/non-existent-device"));
}

TEST(UtilsTest, GetPartitionNumberTest) {
  EXPECT_EQ(3, utils::GetPartitionNumber("/dev/sda3"));
  EXPECT_EQ(3, utils::GetPartitionNumber("/dev/sdz3"));
  EXPECT_EQ(123, utils::GetPartitionNumber("/dev/sda123"));
  EXPECT_EQ(2, utils::GetPartitionNumber("/dev/mmcblk0p2"));
  EXPECT_EQ(0, utils::GetPartitionNumber("/dev/mmcblk0p"));
  EXPECT_EQ(3, utils::GetPartitionNumber("/dev/ubiblock3_2"));
  EXPECT_EQ(0, utils::GetPartitionNumber(""));
  EXPECT_EQ(0, utils::GetPartitionNumber("/"));
  EXPECT_EQ(0, utils::GetPartitionNumber("/dev/"));
  EXPECT_EQ(0, utils::GetPartitionNumber("/dev/sda"));
  EXPECT_EQ(10, utils::GetPartitionNumber("/dev/loop10"));
  EXPECT_EQ(11, utils::GetPartitionNumber("/dev/loop28p11"));
  EXPECT_EQ(10, utils::GetPartitionNumber("/dev/loop10_0"));
  EXPECT_EQ(11, utils::GetPartitionNumber("/dev/loop28p11_0"));
}

TEST(UtilsTest, MakePartitionNameTest) {
  EXPECT_EQ("/dev/sda4", utils::MakePartitionName("/dev/sda", 4));
  EXPECT_EQ("/dev/sda123", utils::MakePartitionName("/dev/sda", 123));
  EXPECT_EQ("/dev/mmcblk2", utils::MakePartitionName("/dev/mmcblk", 2));
  EXPECT_EQ("/dev/mmcblk0p2", utils::MakePartitionName("/dev/mmcblk0", 2));
  EXPECT_EQ("/dev/loop8", utils::MakePartitionName("/dev/loop", 8));
  EXPECT_EQ("/dev/loop12p2", utils::MakePartitionName("/dev/loop12", 2));
  EXPECT_EQ("/dev/ubi5_0", utils::MakePartitionName("/dev/ubiblock", 5));
  EXPECT_EQ("/dev/mtd4", utils::MakePartitionName("/dev/ubiblock", 4));
  EXPECT_EQ("/dev/ubi3_0", utils::MakePartitionName("/dev/ubiblock", 3));
  EXPECT_EQ("/dev/mtd2", utils::MakePartitionName("/dev/ubiblock", 2));
  EXPECT_EQ("/dev/ubi1_0", utils::MakePartitionName("/dev/ubiblock", 1));
}

TEST(UtilsTest, MakePartitionNameForMountTest) {
  EXPECT_EQ("/dev/sda4", utils::MakePartitionNameForMount("/dev/sda4"));
  EXPECT_EQ("/dev/sda123", utils::MakePartitionNameForMount("/dev/sda123"));
  EXPECT_EQ("/dev/mmcblk2", utils::MakePartitionNameForMount("/dev/mmcblk2"));
  EXPECT_EQ("/dev/mmcblk0p2",
            utils::MakePartitionNameForMount("/dev/mmcblk0p2"));
  EXPECT_EQ("/dev/loop0", utils::MakePartitionNameForMount("/dev/loop0"));
  EXPECT_EQ("/dev/loop8", utils::MakePartitionNameForMount("/dev/loop8"));
  EXPECT_EQ("/dev/loop12p2",
            utils::MakePartitionNameForMount("/dev/loop12p2"));
  EXPECT_EQ("/dev/ubiblock5_0",
            utils::MakePartitionNameForMount("/dev/ubiblock5_0"));
  EXPECT_EQ("/dev/mtd4",
            utils::MakePartitionNameForMount("/dev/ubi4_0"));
  EXPECT_EQ("/dev/ubiblock3_0",
            utils::MakePartitionNameForMount("/dev/ubiblock3"));
  EXPECT_EQ("/dev/mtd2", utils::MakePartitionNameForMount("/dev/ubi2"));
  EXPECT_EQ("/dev/ubi1_0",
            utils::MakePartitionNameForMount("/dev/ubiblock1"));
}

namespace {
// Compares cpu shares and returns an integer that is less
// than, equal to or greater than 0 if |shares_lhs| is,
// respectively, lower than, same as or higher than |shares_rhs|.
int CompareCpuShares(utils::CpuShares shares_lhs,
                     utils::CpuShares shares_rhs) {
  return static_cast<int>(shares_lhs) - static_cast<int>(shares_rhs);
}
}  // namespace

// Tests the CPU shares enum is in the order we expect it.
TEST(UtilsTest, CompareCpuSharesTest) {
  EXPECT_LT(CompareCpuShares(utils::kCpuSharesLow,
                             utils::kCpuSharesNormal), 0);
  EXPECT_GT(CompareCpuShares(utils::kCpuSharesNormal,
                             utils::kCpuSharesLow), 0);
  EXPECT_EQ(CompareCpuShares(utils::kCpuSharesNormal,
                             utils::kCpuSharesNormal), 0);
  EXPECT_GT(CompareCpuShares(utils::kCpuSharesHigh,
                             utils::kCpuSharesNormal), 0);
}

TEST(UtilsTest, FuzzIntTest) {
  static const unsigned int kRanges[] = { 0, 1, 2, 20 };
  for (unsigned int range : kRanges) {
    const int kValue = 50;
    for (int tries = 0; tries < 100; ++tries) {
      int value = utils::FuzzInt(kValue, range);
      EXPECT_GE(value, kValue - range / 2);
      EXPECT_LE(value, kValue + range - range / 2);
    }
  }
}

TEST(UtilsTest, ApplyMapTest) {
  int initial_values[] = {1, 2, 3, 4, 6};
  vector<int> collection(std::begin(initial_values), std::end(initial_values));
  EXPECT_EQ(arraysize(initial_values), collection.size());
  int expected_values[] = {1, 2, 5, 4, 8};
  map<int, int> value_map;
  value_map[3] = 5;
  value_map[6] = 8;
  value_map[5] = 10;

  utils::ApplyMap(&collection, value_map);

  size_t index = 0;
  for (const int value : collection) {
    EXPECT_EQ(expected_values[index++], value);
  }
}

TEST(UtilsTest, RunAsRootGetFilesystemSizeTest) {
  string img;
  EXPECT_TRUE(utils::MakeTempFile("img.XXXXXX", &img, nullptr));
  ScopedPathUnlinker img_unlinker(img);
  test_utils::CreateExtImageAtPath(img, nullptr);
  // Extend the "partition" holding the file system from 10MiB to 20MiB.
  EXPECT_EQ(0, test_utils::System(base::StringPrintf(
      "dd if=/dev/zero of=%s seek=20971519 bs=1 count=1 status=none",
      img.c_str())));
  EXPECT_EQ(20 * 1024 * 1024, utils::FileSize(img));
  int block_count = 0;
  int block_size = 0;
  EXPECT_TRUE(utils::GetFilesystemSize(img, &block_count, &block_size));
  EXPECT_EQ(4096, block_size);
  EXPECT_EQ(10 * 1024 * 1024 / 4096, block_count);
}

// Squashfs example filesystem, generated with:
//   echo hola>hola
//   mksquashfs hola hola.sqfs -noappend -nopad
//   hexdump hola.sqfs -e '16/1 "%02x, " "\n"'
const uint8_t kSquashfsFile[] = {
  0x68, 0x73, 0x71, 0x73, 0x02, 0x00, 0x00, 0x00,  // magic, inodes
  0x3e, 0x49, 0x61, 0x54, 0x00, 0x00, 0x02, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11, 0x00,
  0xc0, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00,  // flags, noids, major, minor
  0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // root_inode
  0xef, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // bytes_used
  0xe7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x93, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xbd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xd5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x68, 0x6f, 0x6c, 0x61, 0x0a, 0x2c, 0x00, 0x78,
  0xda, 0x63, 0x62, 0x58, 0xc2, 0xc8, 0xc0, 0xc0,
  0xc8, 0xd0, 0x6b, 0x91, 0x18, 0x02, 0x64, 0xa0,
  0x00, 0x56, 0x06, 0x90, 0xcc, 0x7f, 0xb0, 0xbc,
  0x9d, 0x67, 0x62, 0x08, 0x13, 0x54, 0x1c, 0x44,
  0x4b, 0x03, 0x31, 0x33, 0x10, 0x03, 0x00, 0xb5,
  0x87, 0x04, 0x89, 0x16, 0x00, 0x78, 0xda, 0x63,
  0x60, 0x80, 0x00, 0x46, 0x28, 0xcd, 0xc4, 0xc0,
  0xcc, 0x90, 0x91, 0x9f, 0x93, 0x08, 0x00, 0x04,
  0x70, 0x01, 0xab, 0x10, 0x80, 0x60, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0xab, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x78,
  0xda, 0x63, 0x60, 0x80, 0x00, 0x05, 0x28, 0x0d,
  0x00, 0x01, 0x10, 0x00, 0x21, 0xc5, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x80, 0x99,
  0xcd, 0x02, 0x00, 0x88, 0x13, 0x00, 0x00, 0xdd,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

TEST(UtilsTest, GetSquashfs4Size) {
  uint8_t buffer[sizeof(kSquashfsFile)];
  memcpy(buffer, kSquashfsFile, sizeof(kSquashfsFile));

  int block_count = -1;
  int block_size = -1;
  // Not enough bytes passed.
  EXPECT_FALSE(utils::GetSquashfs4Size(buffer, 10, nullptr, nullptr));

  // The whole file system is passed, which is enough for parsing.
  EXPECT_TRUE(utils::GetSquashfs4Size(buffer, sizeof(kSquashfsFile),
                                      &block_count, &block_size));
  EXPECT_EQ(4096, block_size);
  EXPECT_EQ(1, block_count);

  // Modify the major version to 5.
  uint16_t* s_major = reinterpret_cast<uint16_t*>(buffer + 0x1c);
  *s_major = 5;
  EXPECT_FALSE(utils::GetSquashfs4Size(buffer, 10, nullptr, nullptr));
  memcpy(buffer, kSquashfsFile, sizeof(kSquashfsFile));

  // Modify the bytes_used to have 6 blocks.
  int64_t* bytes_used = reinterpret_cast<int64_t*>(buffer + 0x28);
  *bytes_used = 4096 * 5 + 1;  // 6 "blocks".
  EXPECT_TRUE(utils::GetSquashfs4Size(buffer, sizeof(kSquashfsFile),
                                      &block_count, &block_size));
  EXPECT_EQ(4096, block_size);
  EXPECT_EQ(6, block_count);
}

TEST(UtilsTest, GetInstallDevTest) {
  string boot_dev = "/dev/sda5";
  string install_dev;
  EXPECT_TRUE(utils::GetInstallDev(boot_dev, &install_dev));
  EXPECT_EQ(install_dev, "/dev/sda3");

  boot_dev = "/dev/sda3";
  EXPECT_TRUE(utils::GetInstallDev(boot_dev, &install_dev));
  EXPECT_EQ(install_dev, "/dev/sda5");

  boot_dev = "/dev/sda12";
  EXPECT_FALSE(utils::GetInstallDev(boot_dev, &install_dev));

  boot_dev = "/dev/ubiblock3_0";
  EXPECT_TRUE(utils::GetInstallDev(boot_dev, &install_dev));
  EXPECT_EQ(install_dev, "/dev/ubi5_0");

  boot_dev = "/dev/ubiblock5_0";
  EXPECT_TRUE(utils::GetInstallDev(boot_dev, &install_dev));
  EXPECT_EQ(install_dev, "/dev/ubi3_0");

  boot_dev = "/dev/ubiblock12_0";
  EXPECT_FALSE(utils::GetInstallDev(boot_dev, &install_dev));
}

namespace {
void GetFileFormatTester(const string& expected,
                         const vector<uint8_t>& contents) {
  test_utils::ScopedTempFile file;
  ASSERT_TRUE(utils::WriteFile(file.GetPath().c_str(),
                               reinterpret_cast<const char*>(contents.data()),
                               contents.size()));
  EXPECT_EQ(expected, utils::GetFileFormat(file.GetPath()));
}
}  // namespace

TEST(UtilsTest, GetFileFormatTest) {
  EXPECT_EQ("File not found.", utils::GetFileFormat("/path/to/nowhere"));
  GetFileFormatTester("data", vector<uint8_t>{1, 2, 3, 4, 5, 6, 7, 8});
  GetFileFormatTester("ELF", vector<uint8_t>{0x7f, 0x45, 0x4c, 0x46});

  // Real tests from cros_installer on different boards.
  // ELF 32-bit LSB executable, Intel 80386
  GetFileFormatTester(
      "ELF 32-bit little-endian x86",
      vector<uint8_t>{0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x02, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00,
                      0x90, 0x83, 0x04, 0x08, 0x34, 0x00, 0x00, 0x00});

  // ELF 32-bit LSB executable, MIPS
  GetFileFormatTester(
      "ELF 32-bit little-endian mips",
      vector<uint8_t>{0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x03, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00,
                      0xc0, 0x12, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00});

  // ELF 32-bit LSB executable, ARM
  GetFileFormatTester(
      "ELF 32-bit little-endian arm",
      vector<uint8_t>{0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x02, 0x00, 0x28, 0x00, 0x01, 0x00, 0x00, 0x00,
                      0x85, 0x8b, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00});

  // ELF 64-bit LSB executable, x86-64
  GetFileFormatTester(
      "ELF 64-bit little-endian x86-64",
      vector<uint8_t>{0x7f, 0x45, 0x4c, 0x46, 0x02, 0x01, 0x01, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x02, 0x00, 0x3e, 0x00, 0x01, 0x00, 0x00, 0x00,
                      0xb0, 0x04, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00});
}

namespace {
gboolean  TerminateScheduleCrashReporterUploadTest(void* arg) {
  GMainLoop* loop = reinterpret_cast<GMainLoop*>(arg);
  g_main_loop_quit(loop);
  return FALSE;  // Don't call this callback again
}
}  // namespace

TEST(UtilsTest, ScheduleCrashReporterUploadTest) {
  // Not much to test. At least this tests for memory leaks, crashes,
  // log errors.
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  utils::ScheduleCrashReporterUpload();
  g_timeout_add_seconds(1, &TerminateScheduleCrashReporterUploadTest, loop);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

TEST(UtilsTest, FormatTimeDeltaTest) {
  // utils::FormatTimeDelta() is not locale-aware (it's only used for logging
  // which is not localized) so we only need to test the C locale
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromMilliseconds(100)),
            "0.1s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(0)),
            "0s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(1)),
            "1s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(59)),
            "59s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(60)),
            "1m0s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(61)),
            "1m1s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(90)),
            "1m30s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(1205)),
            "20m5s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(3600)),
            "1h0m0s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(3601)),
            "1h0m1s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(3661)),
            "1h1m1s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(7261)),
            "2h1m1s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(86400)),
            "1d0h0m0s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(86401)),
            "1d0h0m1s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(200000)),
            "2d7h33m20s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(200000) +
                                   base::TimeDelta::FromMilliseconds(1)),
            "2d7h33m20.001s");
  EXPECT_EQ(utils::FormatTimeDelta(base::TimeDelta::FromSeconds(-1)),
            "-1s");
}

TEST(UtilsTest, TimeFromStructTimespecTest) {
  struct timespec ts;

  // Unix epoch (Thursday 00:00:00 UTC on Jan 1, 1970)
  ts = (struct timespec) {.tv_sec = 0, .tv_nsec = 0};
  EXPECT_EQ(base::Time::UnixEpoch(), utils::TimeFromStructTimespec(&ts));

  // 42 ms after the Unix billennium (Sunday 01:46:40 UTC on September 9, 2001)
  ts = (struct timespec) {.tv_sec = 1000 * 1000 * 1000,
                          .tv_nsec = 42 * 1000 * 1000};
  base::Time::Exploded exploded = (base::Time::Exploded) {
    .year = 2001, .month = 9, .day_of_week = 0, .day_of_month = 9,
    .hour = 1, .minute = 46, .second = 40, .millisecond = 42};
  EXPECT_EQ(base::Time::FromUTCExploded(exploded),
            utils::TimeFromStructTimespec(&ts));
}

TEST(UtilsTest, DecodeAndStoreBase64String) {
  base::FilePath path;

  // Ensure we return false on empty strings or invalid base64.
  EXPECT_FALSE(utils::DecodeAndStoreBase64String("", &path));
  EXPECT_FALSE(utils::DecodeAndStoreBase64String("not valid base64", &path));

  // Pass known base64 and check that it matches. This string was generated
  // the following way:
  //
  //   $ echo "Update Engine" | base64
  //   VXBkYXRlIEVuZ2luZQo=
  EXPECT_TRUE(utils::DecodeAndStoreBase64String("VXBkYXRlIEVuZ2luZQo=",
                                                &path));
  ScopedPathUnlinker unlinker(path.value());
  string expected_contents = "Update Engine\n";
  string contents;
  EXPECT_TRUE(utils::ReadFile(path.value(), &contents));
  EXPECT_EQ(contents, expected_contents);
  EXPECT_EQ(utils::FileSize(path.value()), expected_contents.size());
}

TEST(UtilsTest, ConvertToOmahaInstallDate) {
  // The Omaha Epoch starts at Jan 1, 2007 0:00 PST which is a
  // Monday. In Unix time, this point in time is easily obtained via
  // the date(1) command like this:
  //
  //  $ date +"%s" --date="Jan 1, 2007 0:00 PST"
  const time_t omaha_epoch = 1167638400;
  int value;

  // Points in time *on and after* the Omaha epoch should not fail.
  EXPECT_TRUE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(omaha_epoch), &value));
  EXPECT_GE(value, 0);

  // Anything before the Omaha epoch should fail. We test it for two points.
  EXPECT_FALSE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(omaha_epoch - 1), &value));
  EXPECT_FALSE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(omaha_epoch - 100*24*3600), &value));

  // Check that we jump from 0 to 7 exactly on the one-week mark, e.g.
  // on Jan 8, 2007 0:00 PST.
  EXPECT_TRUE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(omaha_epoch + 7*24*3600 - 1), &value));
  EXPECT_EQ(value, 0);
  EXPECT_TRUE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(omaha_epoch + 7*24*3600), &value));
  EXPECT_EQ(value, 7);

  // Check a couple of more values.
  EXPECT_TRUE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(omaha_epoch + 10*24*3600), &value));
  EXPECT_EQ(value, 7);
  EXPECT_TRUE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(omaha_epoch + 20*24*3600), &value));
  EXPECT_EQ(value, 14);
  EXPECT_TRUE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(omaha_epoch + 26*24*3600), &value));
  EXPECT_EQ(value, 21);
  EXPECT_TRUE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(omaha_epoch + 29*24*3600), &value));
  EXPECT_EQ(value, 28);

  // The date Jun 4, 2007 0:00 PDT is a Monday and is hence a point
  // where the Omaha InstallDate jumps 7 days. Its unix time is
  // 1180940400. Notably, this is a point in time where Daylight
  // Savings Time (DST) was is in effect (e.g. it's PDT, not PST).
  //
  // Note that as utils::ConvertToOmahaInstallDate() _deliberately_
  // ignores DST (as it's hard to implement in a thread-safe way using
  // glibc, see comments in utils.h) we have to fudge by the DST
  // offset which is one hour. Conveniently, if the function were
  // someday modified to be DST aware, this test would have to be
  // modified as well.
  const time_t dst_time = 1180940400;  // Jun 4, 2007 0:00 PDT.
  const time_t fudge = 3600;
  int value1, value2;
  EXPECT_TRUE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(dst_time + fudge - 1), &value1));
  EXPECT_TRUE(utils::ConvertToOmahaInstallDate(
      base::Time::FromTimeT(dst_time + fudge), &value2));
  EXPECT_EQ(value1, value2 - 7);
}

TEST(UtilsTest, WallclockDurationHelper) {
  FakeSystemState fake_system_state;
  FakeClock fake_clock;
  base::TimeDelta duration;
  string state_variable_key = "test-prefs";
  FakePrefs fake_prefs;

  fake_system_state.set_clock(&fake_clock);
  fake_system_state.set_prefs(&fake_prefs);

  // Initialize wallclock to 1 sec.
  fake_clock.SetWallclockTime(base::Time::FromInternalValue(1000000));

  // First time called so no previous measurement available.
  EXPECT_FALSE(utils::WallclockDurationHelper(&fake_system_state,
                                              state_variable_key,
                                              &duration));

  // Next time, we should get zero since the clock didn't advance.
  EXPECT_TRUE(utils::WallclockDurationHelper(&fake_system_state,
                                             state_variable_key,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 0);

  // We can also call it as many times as we want with it being
  // considered a failure.
  EXPECT_TRUE(utils::WallclockDurationHelper(&fake_system_state,
                                             state_variable_key,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 0);
  EXPECT_TRUE(utils::WallclockDurationHelper(&fake_system_state,
                                             state_variable_key,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 0);

  // Advance the clock one second, then we should get 1 sec on the
  // next call and 0 sec on the subsequent call.
  fake_clock.SetWallclockTime(base::Time::FromInternalValue(2000000));
  EXPECT_TRUE(utils::WallclockDurationHelper(&fake_system_state,
                                             state_variable_key,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 1);
  EXPECT_TRUE(utils::WallclockDurationHelper(&fake_system_state,
                                             state_variable_key,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 0);

  // Advance clock two seconds and we should get 2 sec and then 0 sec.
  fake_clock.SetWallclockTime(base::Time::FromInternalValue(4000000));
  EXPECT_TRUE(utils::WallclockDurationHelper(&fake_system_state,
                                             state_variable_key,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 2);
  EXPECT_TRUE(utils::WallclockDurationHelper(&fake_system_state,
                                             state_variable_key,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 0);

  // There's a possibility that the wallclock can go backwards (NTP
  // adjustments, for example) so check that we properly handle this
  // case.
  fake_clock.SetWallclockTime(base::Time::FromInternalValue(3000000));
  EXPECT_FALSE(utils::WallclockDurationHelper(&fake_system_state,
                                              state_variable_key,
                                              &duration));
  fake_clock.SetWallclockTime(base::Time::FromInternalValue(4000000));
  EXPECT_TRUE(utils::WallclockDurationHelper(&fake_system_state,
                                             state_variable_key,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 1);
}

TEST(UtilsTest, MonotonicDurationHelper) {
  int64_t storage = 0;
  FakeSystemState fake_system_state;
  FakeClock fake_clock;
  base::TimeDelta duration;

  fake_system_state.set_clock(&fake_clock);

  // Initialize monotonic clock to 1 sec.
  fake_clock.SetMonotonicTime(base::Time::FromInternalValue(1000000));

  // First time called so no previous measurement available.
  EXPECT_FALSE(utils::MonotonicDurationHelper(&fake_system_state,
                                              &storage,
                                              &duration));

  // Next time, we should get zero since the clock didn't advance.
  EXPECT_TRUE(utils::MonotonicDurationHelper(&fake_system_state,
                                             &storage,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 0);

  // We can also call it as many times as we want with it being
  // considered a failure.
  EXPECT_TRUE(utils::MonotonicDurationHelper(&fake_system_state,
                                             &storage,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 0);
  EXPECT_TRUE(utils::MonotonicDurationHelper(&fake_system_state,
                                             &storage,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 0);

  // Advance the clock one second, then we should get 1 sec on the
  // next call and 0 sec on the subsequent call.
  fake_clock.SetMonotonicTime(base::Time::FromInternalValue(2000000));
  EXPECT_TRUE(utils::MonotonicDurationHelper(&fake_system_state,
                                             &storage,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 1);
  EXPECT_TRUE(utils::MonotonicDurationHelper(&fake_system_state,
                                             &storage,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 0);

  // Advance clock two seconds and we should get 2 sec and then 0 sec.
  fake_clock.SetMonotonicTime(base::Time::FromInternalValue(4000000));
  EXPECT_TRUE(utils::MonotonicDurationHelper(&fake_system_state,
                                             &storage,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 2);
  EXPECT_TRUE(utils::MonotonicDurationHelper(&fake_system_state,
                                             &storage,
                                             &duration));
  EXPECT_EQ(duration.InSeconds(), 0);
}

TEST(UtilsTest, GetConnectionType) {
  // Check that expected combinations map to the right value.
  EXPECT_EQ(metrics::ConnectionType::kUnknown,
            utils::GetConnectionType(kNetUnknown,
                                     NetworkTethering::kUnknown));
  EXPECT_EQ(metrics::ConnectionType::kEthernet,
            utils::GetConnectionType(kNetEthernet,
                                     NetworkTethering::kUnknown));
  EXPECT_EQ(metrics::ConnectionType::kWifi,
            utils::GetConnectionType(kNetWifi,
                                     NetworkTethering::kUnknown));
  EXPECT_EQ(metrics::ConnectionType::kWimax,
            utils::GetConnectionType(kNetWimax,
                                     NetworkTethering::kUnknown));
  EXPECT_EQ(metrics::ConnectionType::kBluetooth,
            utils::GetConnectionType(kNetBluetooth,
                                     NetworkTethering::kUnknown));
  EXPECT_EQ(metrics::ConnectionType::kCellular,
            utils::GetConnectionType(kNetCellular,
                                     NetworkTethering::kUnknown));
  EXPECT_EQ(metrics::ConnectionType::kTetheredEthernet,
            utils::GetConnectionType(kNetEthernet,
                                     NetworkTethering::kConfirmed));
  EXPECT_EQ(metrics::ConnectionType::kTetheredWifi,
            utils::GetConnectionType(kNetWifi,
                                     NetworkTethering::kConfirmed));

  // Ensure that we don't report tethered ethernet unless it's confirmed.
  EXPECT_EQ(metrics::ConnectionType::kEthernet,
            utils::GetConnectionType(kNetEthernet,
                                     NetworkTethering::kNotDetected));
  EXPECT_EQ(metrics::ConnectionType::kEthernet,
            utils::GetConnectionType(kNetEthernet,
                                     NetworkTethering::kSuspected));
  EXPECT_EQ(metrics::ConnectionType::kEthernet,
            utils::GetConnectionType(kNetEthernet,
                                     NetworkTethering::kUnknown));

  // Ditto for tethered wifi.
  EXPECT_EQ(metrics::ConnectionType::kWifi,
            utils::GetConnectionType(kNetWifi,
                                     NetworkTethering::kNotDetected));
  EXPECT_EQ(metrics::ConnectionType::kWifi,
            utils::GetConnectionType(kNetWifi,
                                     NetworkTethering::kSuspected));
  EXPECT_EQ(metrics::ConnectionType::kWifi,
            utils::GetConnectionType(kNetWifi,
                                     NetworkTethering::kUnknown));
}

TEST(UtilsTest, GetMinorVersion) {
  // Test GetMinorVersion by verifying that it parses the conf file and returns
  // the correct value.
  string contents = "PAYLOAD_MINOR_VERSION=1\n";
  uint32_t minor_version;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath temp_file("update_engine.conf");
  base::FilePath filepath = temp_dir.path().Append(temp_file);

  ASSERT_TRUE(test_utils::WriteFileString(filepath.value(), contents.c_str()));
  ASSERT_TRUE(utils::GetMinorVersion(filepath, &minor_version));
  ASSERT_EQ(minor_version, 1);
}

static bool BoolMacroTestHelper() {
  int i = 1;
  unsigned int ui = 1;
  bool b = 1;
  std::unique_ptr<char> cptr(new char);

  TEST_AND_RETURN_FALSE(i);
  TEST_AND_RETURN_FALSE(ui);
  TEST_AND_RETURN_FALSE(b);
  TEST_AND_RETURN_FALSE(cptr);

  TEST_AND_RETURN_FALSE_ERRNO(i);
  TEST_AND_RETURN_FALSE_ERRNO(ui);
  TEST_AND_RETURN_FALSE_ERRNO(b);
  TEST_AND_RETURN_FALSE_ERRNO(cptr);

  return true;
}

static void VoidMacroTestHelper(bool* ret) {
  int i = 1;
  unsigned int ui = 1;
  bool b = 1;
  std::unique_ptr<char> cptr(new char);

  *ret = false;

  TEST_AND_RETURN(i);
  TEST_AND_RETURN(ui);
  TEST_AND_RETURN(b);
  TEST_AND_RETURN(cptr);

  TEST_AND_RETURN_ERRNO(i);
  TEST_AND_RETURN_ERRNO(ui);
  TEST_AND_RETURN_ERRNO(b);
  TEST_AND_RETURN_ERRNO(cptr);

  *ret = true;
}

TEST(UtilsTest, TestMacros) {
  bool void_test = false;
  VoidMacroTestHelper(&void_test);
  EXPECT_TRUE(void_test);

  EXPECT_TRUE(BoolMacroTestHelper());
}

}  // namespace chromeos_update_engine
