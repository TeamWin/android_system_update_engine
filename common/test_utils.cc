//
// Copyright (C) 2012 The Android Open Source Project
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

#include "update_engine/common/test_utils.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/format_macros.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "update_engine/common/error_code_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/file_writer.h"

using base::StringPrintf;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

void PrintTo(const Extent& extent, ::std::ostream* os) {
  *os << "(" << extent.start_block() << ", " << extent.num_blocks() << ")";
}

void PrintTo(const ErrorCode& error_code, ::std::ostream* os) {
  *os << utils::ErrorCodeToString(error_code);
}

namespace test_utils {

const char* const kMountPathTemplate = "UpdateEngineTests_mnt-XXXXXX";

const uint8_t kRandomString[] = {
  0xf2, 0xb7, 0x55, 0x92, 0xea, 0xa6, 0xc9, 0x57,
  0xe0, 0xf8, 0xeb, 0x34, 0x93, 0xd9, 0xc4, 0x8f,
  0xcb, 0x20, 0xfa, 0x37, 0x4b, 0x40, 0xcf, 0xdc,
  0xa5, 0x08, 0x70, 0x89, 0x79, 0x35, 0xe2, 0x3d,
  0x56, 0xa4, 0x75, 0x73, 0xa3, 0x6d, 0xd1, 0xd5,
  0x26, 0xbb, 0x9c, 0x60, 0xbd, 0x2f, 0x5a, 0xfa,
  0xb7, 0xd4, 0x3a, 0x50, 0xa7, 0x6b, 0x3e, 0xfd,
  0x61, 0x2b, 0x3a, 0x31, 0x30, 0x13, 0x33, 0x53,
  0xdb, 0xd0, 0x32, 0x71, 0x5c, 0x39, 0xed, 0xda,
  0xb4, 0x84, 0xca, 0xbc, 0xbd, 0x78, 0x1c, 0x0c,
  0xd8, 0x0b, 0x41, 0xe8, 0xe1, 0xe0, 0x41, 0xad,
  0x03, 0x12, 0xd3, 0x3d, 0xb8, 0x75, 0x9b, 0xe6,
  0xd9, 0x01, 0xd0, 0x87, 0xf4, 0x36, 0xfa, 0xa7,
  0x0a, 0xfa, 0xc5, 0x87, 0x65, 0xab, 0x9a, 0x7b,
  0xeb, 0x58, 0x23, 0xf0, 0xa8, 0x0a, 0xf2, 0x33,
  0x3a, 0xe2, 0xe3, 0x35, 0x74, 0x95, 0xdd, 0x3c,
  0x59, 0x5a, 0xd9, 0x52, 0x3a, 0x3c, 0xac, 0xe5,
  0x15, 0x87, 0x6d, 0x82, 0xbc, 0xf8, 0x7d, 0xbe,
  0xca, 0xd3, 0x2c, 0xd6, 0xec, 0x38, 0xeb, 0xe4,
  0x53, 0xb0, 0x4c, 0x3f, 0x39, 0x29, 0xf7, 0xa4,
  0x73, 0xa8, 0xcb, 0x32, 0x50, 0x05, 0x8c, 0x1c,
  0x1c, 0xca, 0xc9, 0x76, 0x0b, 0x8f, 0x6b, 0x57,
  0x1f, 0x24, 0x2b, 0xba, 0x82, 0xba, 0xed, 0x58,
  0xd8, 0xbf, 0xec, 0x06, 0x64, 0x52, 0x6a, 0x3f,
  0xe4, 0xad, 0xce, 0x84, 0xb4, 0x27, 0x55, 0x14,
  0xe3, 0x75, 0x59, 0x73, 0x71, 0x51, 0xea, 0xe8,
  0xcc, 0xda, 0x4f, 0x09, 0xaf, 0xa4, 0xbc, 0x0e,
  0xa6, 0x1f, 0xe2, 0x3a, 0xf8, 0x96, 0x7d, 0x30,
  0x23, 0xc5, 0x12, 0xb5, 0xd8, 0x73, 0x6b, 0x71,
  0xab, 0xf1, 0xd7, 0x43, 0x58, 0xa7, 0xc9, 0xf0,
  0xe4, 0x85, 0x1c, 0xd6, 0x92, 0x50, 0x2c, 0x98,
  0x36, 0xfe, 0x87, 0xaf, 0x43, 0x8f, 0x8f, 0xf5,
  0x88, 0x48, 0x18, 0x42, 0xcf, 0x42, 0xc1, 0xa8,
  0xe8, 0x05, 0x08, 0xa1, 0x45, 0x70, 0x5b, 0x8c,
  0x39, 0x28, 0xab, 0xe9, 0x6b, 0x51, 0xd2, 0xcb,
  0x30, 0x04, 0xea, 0x7d, 0x2f, 0x6e, 0x6c, 0x3b,
  0x5f, 0x82, 0xd9, 0x5b, 0x89, 0x37, 0x65, 0x65,
  0xbe, 0x9f, 0xa3, 0x5d,
};

bool IsXAttrSupported(const base::FilePath& dir_path) {
  char *path = strdup(dir_path.Append("xattr_test_XXXXXX").value().c_str());

  int fd = mkstemp(path);
  if (fd == -1) {
    PLOG(ERROR) << "Error creating temporary file in " << dir_path.value();
    free(path);
    return false;
  }

  if (unlink(path) != 0) {
    PLOG(ERROR) << "Error unlinking temporary file " << path;
    close(fd);
    free(path);
    return false;
  }

  int xattr_res = fsetxattr(fd, "user.xattr-test", "value", strlen("value"), 0);
  if (xattr_res != 0) {
    if (errno == ENOTSUP) {
      // Leave it to call-sites to warn about non-support.
    } else {
      PLOG(ERROR) << "Error setting xattr on " << path;
    }
  }
  close(fd);
  free(path);
  return xattr_res == 0;
}

bool WriteFileVector(const string& path, const brillo::Blob& data) {
  return utils::WriteFile(path.c_str(), data.data(), data.size());
}

bool WriteFileString(const string& path, const string& data) {
  return utils::WriteFile(path.c_str(), data.data(), data.size());
}

// Binds provided |filename| to an unused loopback device, whose name is written
// to the string pointed to by |lo_dev_name_p|. Returns true on success, false
// otherwise (along with corresponding test failures), in which case the content
// of |lo_dev_name_p| is unknown.
bool BindToUnusedLoopDevice(const string& filename, string* lo_dev_name_p) {
  CHECK(lo_dev_name_p);

  // Bind to an unused loopback device, sanity check the device name.
  lo_dev_name_p->clear();
  if (!(utils::ReadPipe("losetup --show -f " + filename, lo_dev_name_p) &&
        base::StartsWith(*lo_dev_name_p, "/dev/loop",
                         base::CompareCase::SENSITIVE))) {
    ADD_FAILURE();
    return false;
  }

  // Strip anything from the first newline char.
  size_t newline_pos = lo_dev_name_p->find('\n');
  if (newline_pos != string::npos)
    lo_dev_name_p->erase(newline_pos);

  return true;
}

bool ExpectVectorsEq(const brillo::Blob& expected,
                     const brillo::Blob& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  if (expected.size() != actual.size())
    return false;
  bool is_all_eq = true;
  for (unsigned int i = 0; i < expected.size(); i++) {
    EXPECT_EQ(expected[i], actual[i]) << "offset: " << i;
    is_all_eq = is_all_eq && (expected[i] == actual[i]);
  }
  return is_all_eq;
}

void FillWithData(brillo::Blob* buffer) {
  size_t input_counter = 0;
  for (uint8_t& b : *buffer) {
    b = kRandomString[input_counter];
    input_counter++;
    input_counter %= sizeof(kRandomString);
  }
}

void CreateEmptyExtImageAtPath(const string& path,
                               size_t size,
                               int block_size) {
  EXPECT_EQ(0, System(StringPrintf("dd if=/dev/zero of=%s"
                                   " seek=%" PRIuS " bs=1 count=1 status=none",
                                   path.c_str(), size)));
  EXPECT_EQ(0, System(StringPrintf("mkfs.ext3 -q -b %d -F %s",
                                   block_size, path.c_str())));
}

void CreateExtImageAtPath(const string& path, vector<string>* out_paths) {
  // create 10MiB sparse file, mounted at a unique location.
  string mount_path;
  CHECK(utils::MakeTempDirectory(kMountPathTemplate, &mount_path));
  ScopedDirRemover mount_path_unlinker(mount_path);

  EXPECT_EQ(0, System(StringPrintf("dd if=/dev/zero of=%s"
                                   " seek=10485759 bs=1 count=1 status=none",
                                   path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mkfs.ext3 -q -b 4096 -F %s",
                                   path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mount -o loop %s %s", path.c_str(),
                                   mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("echo hi > %s/hi", mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("echo hello > %s/hello",
                                   mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mkdir %s/some_dir", mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mkdir %s/some_dir/empty_dir",
                                   mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mkdir %s/some_dir/mnt",
                                   mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("echo T > %s/some_dir/test",
                                   mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mkfifo %s/some_dir/fifo",
                                   mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mknod %s/cdev c 2 3", mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("ln -s /some/target %s/sym",
                                   mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("ln %s/some_dir/test %s/testlink",
                                   mount_path.c_str(), mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("echo T > %s/srchardlink0",
                                   mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("ln %s/srchardlink0 %s/srchardlink1",
                                   mount_path.c_str(), mount_path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("ln -s bogus %s/boguslink",
                                   mount_path.c_str())));
  EXPECT_TRUE(utils::UnmountFilesystem(mount_path.c_str()));

  if (out_paths) {
    out_paths->clear();
    out_paths->push_back("");
    out_paths->push_back("/hi");
    out_paths->push_back("/boguslink");
    out_paths->push_back("/hello");
    out_paths->push_back("/some_dir");
    out_paths->push_back("/some_dir/empty_dir");
    out_paths->push_back("/some_dir/mnt");
    out_paths->push_back("/some_dir/test");
    out_paths->push_back("/some_dir/fifo");
    out_paths->push_back("/cdev");
    out_paths->push_back("/testlink");
    out_paths->push_back("/sym");
    out_paths->push_back("/srchardlink0");
    out_paths->push_back("/srchardlink1");
    out_paths->push_back("/lost+found");
  }
}

ScopedLoopMounter::ScopedLoopMounter(const string& file_path,
                                     string* mnt_path,
                                     unsigned long flags) {  // NOLINT - long
  EXPECT_TRUE(utils::MakeTempDirectory("mnt.XXXXXX", mnt_path));
  dir_remover_.reset(new ScopedDirRemover(*mnt_path));

  string loop_dev;
  loop_binder_.reset(new ScopedLoopbackDeviceBinder(file_path, &loop_dev));

  EXPECT_TRUE(utils::MountFilesystem(loop_dev, *mnt_path, flags));
  unmounter_.reset(new ScopedFilesystemUnmounter(*mnt_path));
}

base::FilePath GetBuildArtifactsPath() {
  base::FilePath exe_path;
  base::ReadSymbolicLink(base::FilePath("/proc/self/exe"), &exe_path);
  return exe_path.DirName();
}

}  // namespace test_utils
}  // namespace chromeos_update_engine
