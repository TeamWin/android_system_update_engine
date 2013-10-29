// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <map>
#include <string>
#include <vector>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <gtest/gtest.h>

#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

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

  EXPECT_EQ("/dev/ubi2", utils::KernelDeviceOfBootDevice("/dev/ubi3"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/ubi4"));

  EXPECT_EQ("/dev/mtdblock2",
            utils::KernelDeviceOfBootDevice("/dev/ubiblock3_0"));
  EXPECT_EQ("/dev/mtdblock4",
            utils::KernelDeviceOfBootDevice("/dev/ubiblock5_0"));
  EXPECT_EQ("/dev/mtdblock6",
            utils::KernelDeviceOfBootDevice("/dev/ubiblock7_0"));
  EXPECT_EQ("", utils::KernelDeviceOfBootDevice("/dev/ubiblock4_0"));
}


TEST(UtilsTest, NormalizePathTest) {
  EXPECT_EQ("", utils::NormalizePath("", false));
  EXPECT_EQ("", utils::NormalizePath("", true));
  EXPECT_EQ("/", utils::NormalizePath("/", false));
  EXPECT_EQ("", utils::NormalizePath("/", true));
  EXPECT_EQ("/", utils::NormalizePath("//", false));
  EXPECT_EQ("", utils::NormalizePath("//", true));
  EXPECT_EQ("foo", utils::NormalizePath("foo", false));
  EXPECT_EQ("foo", utils::NormalizePath("foo", true));
  EXPECT_EQ("/foo/", utils::NormalizePath("/foo//", false));
  EXPECT_EQ("/foo", utils::NormalizePath("/foo//", true));
  EXPECT_EQ("bar/baz/foo/adlr", utils::NormalizePath("bar/baz//foo/adlr",
                                                     false));
  EXPECT_EQ("bar/baz/foo/adlr", utils::NormalizePath("bar/baz//foo/adlr",
                                                     true));
  EXPECT_EQ("/bar/baz/foo/adlr/", utils::NormalizePath("/bar/baz//foo/adlr/",
                                                       false));
  EXPECT_EQ("/bar/baz/foo/adlr", utils::NormalizePath("/bar/baz//foo/adlr/",
                                                      true));
  EXPECT_EQ("\\\\", utils::NormalizePath("\\\\", false));
  EXPECT_EQ("\\\\", utils::NormalizePath("\\\\", true));
  EXPECT_EQ("\\:/;$PATH\n\\", utils::NormalizePath("\\://;$PATH\n\\", false));
  EXPECT_EQ("\\:/;$PATH\n\\", utils::NormalizePath("\\://;$PATH\n\\", true));
  EXPECT_EQ("/spaces s/ ok/s / / /",
            utils::NormalizePath("/spaces s/ ok/s / / /", false));
  EXPECT_EQ("/spaces s/ ok/s / / ",
            utils::NormalizePath("/spaces s/ ok/s / / /", true));
}

TEST(UtilsTest, ReadFileFailure) {
  vector<char> empty;
  EXPECT_FALSE(utils::ReadFile("/this/doesn't/exist", &empty));
}

TEST(UtilsTest, ReadFileChunk) {
  FilePath file;
  EXPECT_TRUE(file_util::CreateTemporaryFile(&file));
  ScopedPathUnlinker unlinker(file.value());
  vector<char> data;
  const size_t kSize = 1024 * 1024;
  for (size_t i = 0; i < kSize; i++) {
    data.push_back(i % 255);
  }
  EXPECT_TRUE(utils::WriteFile(file.value().c_str(), &data[0], data.size()));
  vector<char> in_data;
  EXPECT_TRUE(utils::ReadFileChunk(file.value().c_str(), kSize, 10, &in_data));
  EXPECT_TRUE(in_data.empty());
  EXPECT_TRUE(utils::ReadFileChunk(file.value().c_str(), 0, -1, &in_data));
  EXPECT_TRUE(data == in_data);
  in_data.clear();
  EXPECT_TRUE(utils::ReadFileChunk(file.value().c_str(), 10, 20, &in_data));
  EXPECT_TRUE(vector<char>(data.begin() + 10, data.begin() + 10 + 20) ==
              in_data);
}

TEST(UtilsTest, ErrnoNumberAsStringTest) {
  EXPECT_EQ("No such file or directory", utils::ErrnoNumberAsString(ENOENT));
}

TEST(UtilsTest, StringHasSuffixTest) {
  EXPECT_TRUE(utils::StringHasSuffix("foo", "foo"));
  EXPECT_TRUE(utils::StringHasSuffix("foo", "o"));
  EXPECT_TRUE(utils::StringHasSuffix("", ""));
  EXPECT_TRUE(utils::StringHasSuffix("abcabc", "abc"));
  EXPECT_TRUE(utils::StringHasSuffix("adlrwashere", "ere"));
  EXPECT_TRUE(utils::StringHasSuffix("abcdefgh", "gh"));
  EXPECT_TRUE(utils::StringHasSuffix("abcdefgh", ""));
  EXPECT_FALSE(utils::StringHasSuffix("foo", "afoo"));
  EXPECT_FALSE(utils::StringHasSuffix("", "x"));
  EXPECT_FALSE(utils::StringHasSuffix("abcdefgh", "fg"));
  EXPECT_FALSE(utils::StringHasSuffix("abcdefgh", "ab"));
}

TEST(UtilsTest, StringHasPrefixTest) {
  EXPECT_TRUE(utils::StringHasPrefix("foo", "foo"));
  EXPECT_TRUE(utils::StringHasPrefix("foo", "f"));
  EXPECT_TRUE(utils::StringHasPrefix("", ""));
  EXPECT_TRUE(utils::StringHasPrefix("abcabc", "abc"));
  EXPECT_TRUE(utils::StringHasPrefix("adlrwashere", "adl"));
  EXPECT_TRUE(utils::StringHasPrefix("abcdefgh", "ab"));
  EXPECT_TRUE(utils::StringHasPrefix("abcdefgh", ""));
  EXPECT_FALSE(utils::StringHasPrefix("foo", "fooa"));
  EXPECT_FALSE(utils::StringHasPrefix("", "x"));
  EXPECT_FALSE(utils::StringHasPrefix("abcdefgh", "bc"));
  EXPECT_FALSE(utils::StringHasPrefix("abcdefgh", "gh"));
}

TEST(UtilsTest, RecursiveUnlinkDirTest) {
  string first_dir_name;
  ASSERT_TRUE(utils::MakeTempDirectory("RecursiveUnlinkDirTest-a-XXXXXX",
                                       &first_dir_name));
  ASSERT_EQ(0, Chmod(first_dir_name, 0755));
  string second_dir_name;
  ASSERT_TRUE(utils::MakeTempDirectory("RecursiveUnlinkDirTest-b-XXXXXX",
                                       &second_dir_name));
  ASSERT_EQ(0, Chmod(second_dir_name, 0755));

  EXPECT_EQ(0, Symlink(string("../") + first_dir_name,
                       second_dir_name + "/link"));
  EXPECT_EQ(0, System(string("echo hi > ") + second_dir_name + "/file"));
  EXPECT_EQ(0, Mkdir(second_dir_name + "/dir", 0755));
  EXPECT_EQ(0, System(string("echo ok > ") + second_dir_name + "/dir/subfile"));
  EXPECT_TRUE(utils::RecursiveUnlinkDir(second_dir_name));
  EXPECT_TRUE(utils::FileExists(first_dir_name.c_str()));
  EXPECT_EQ(0, System(string("rm -rf ") + first_dir_name));
  EXPECT_FALSE(utils::FileExists(second_dir_name.c_str()));
  EXPECT_TRUE(utils::RecursiveUnlinkDir("/something/that/doesnt/exist"));
}

TEST(UtilsTest, IsSymlinkTest) {
  string temp_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("/tmp/symlink-test.XXXXXX", &temp_dir));
  string temp_file = temp_dir + "temp-file";
  EXPECT_TRUE(utils::WriteFile(temp_file.c_str(), "", 0));
  string temp_symlink = temp_dir + "temp-symlink";
  EXPECT_EQ(0, symlink(temp_file.c_str(), temp_symlink.c_str()));
  EXPECT_FALSE(utils::IsSymlink(temp_dir.c_str()));
  EXPECT_FALSE(utils::IsSymlink(temp_file.c_str()));
  EXPECT_TRUE(utils::IsSymlink(temp_symlink.c_str()));
  EXPECT_FALSE(utils::IsSymlink("/non/existent/path"));
  EXPECT_TRUE(utils::RecursiveUnlinkDir(temp_dir));
}

TEST(UtilsTest, TempFilenameTest) {
  const string original = "/foo.XXXXXX";
  const string result = utils::TempFilename(original);
  EXPECT_EQ(original.size(), result.size());
  EXPECT_TRUE(utils::StringHasPrefix(result, "/foo."));
  EXPECT_FALSE(utils::StringHasSuffix(result, "XXXXXX"));
}

TEST(UtilsTest, RootDeviceTest) {
  EXPECT_EQ("/dev/sda", utils::RootDevice("/dev/sda3"));
  EXPECT_EQ("/dev/mmc0", utils::RootDevice("/dev/mmc0p3"));
  EXPECT_EQ("", utils::RootDevice("/dev/foo/bar"));
  EXPECT_EQ("", utils::RootDevice("/"));
  EXPECT_EQ("", utils::RootDevice(""));
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

TEST(UtilsTest, PartitionNumberTest) {
  EXPECT_EQ("3", utils::PartitionNumber("/dev/sda3"));
  EXPECT_EQ("3", utils::PartitionNumber("/dev/mmc0p3"));
}

TEST(UtilsTest, CompareCpuSharesTest) {
  EXPECT_LT(utils::CompareCpuShares(utils::kCpuSharesLow,
                                    utils::kCpuSharesNormal), 0);
  EXPECT_GT(utils::CompareCpuShares(utils::kCpuSharesNormal,
                                    utils::kCpuSharesLow), 0);
  EXPECT_EQ(utils::CompareCpuShares(utils::kCpuSharesNormal,
                                    utils::kCpuSharesNormal), 0);
  EXPECT_GT(utils::CompareCpuShares(utils::kCpuSharesHigh,
                                    utils::kCpuSharesNormal), 0);
}

TEST(UtilsTest, FuzzIntTest) {
  static const unsigned int kRanges[] = { 0, 1, 2, 20 };
  for (size_t r = 0; r < arraysize(kRanges); ++r) {
    unsigned int range = kRanges[r];
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
  vector<int> collection(&initial_values[0],
                         initial_values + arraysize(initial_values));
  EXPECT_EQ(arraysize(initial_values), collection.size());
  int expected_values[] = {1, 2, 5, 4, 8};
  map<int, int> value_map;
  value_map[3] = 5;
  value_map[6] = 8;
  value_map[5] = 10;

  utils::ApplyMap(&collection, value_map);

  size_t index = 0;
  for (vector<int>::iterator it = collection.begin(), e = collection.end();
       it != e; ++it) {
    EXPECT_EQ(expected_values[index++], *it);
  }
}

TEST(UtilsTest, RunAsRootGetFilesystemSizeTest) {
  string img;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/img.XXXXXX", &img, NULL));
  ScopedPathUnlinker img_unlinker(img);
  CreateExtImageAtPath(img, NULL);
  // Extend the "partition" holding the file system from 10MiB to 20MiB.
  EXPECT_EQ(0, System(base::StringPrintf(
      "dd if=/dev/zero of=%s seek=20971519 bs=1 count=1",
      img.c_str())));
  EXPECT_EQ(20 * 1024 * 1024, utils::FileSize(img));
  int block_count = 0;
  int block_size = 0;
  EXPECT_TRUE(utils::GetFilesystemSize(img, &block_count, &block_size));
  EXPECT_EQ(4096, block_size);
  EXPECT_EQ(10 * 1024 * 1024 / 4096, block_count);
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
  EXPECT_EQ(install_dev, "/dev/ubiblock5_0");

  boot_dev = "/dev/ubiblock5_0";
  EXPECT_TRUE(utils::GetInstallDev(boot_dev, &install_dev));
  EXPECT_EQ(install_dev, "/dev/ubiblock3_0");

  boot_dev = "/dev/ubiblock12_0";
  EXPECT_FALSE(utils::GetInstallDev(boot_dev, &install_dev));
}

namespace {
gboolean  TerminateScheduleCrashReporterUploadTest(void* arg) {
  GMainLoop* loop = reinterpret_cast<GMainLoop*>(arg);
  g_main_loop_quit(loop);
  return FALSE;  // Don't call this callback again
}
}  // namespace {}

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

}  // namespace chromeos_update_engine
