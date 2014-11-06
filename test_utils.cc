// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/test_utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "update_engine/file_writer.h"
#include "update_engine/payload_generator/filesystem_iterator.h"
#include "update_engine/utils.h"

using base::StringPrintf;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

const char* const kMountPathTemplate = "UpdateEngineTests_mnt-XXXXXX";

bool WriteFileVector(const string& path, const vector<char>& data) {
  return utils::WriteFile(path.c_str(), &data[0], data.size());
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
        StartsWithASCII(*lo_dev_name_p, "/dev/loop", true))) {
    ADD_FAILURE();
    return false;
  }

  // Strip anything from the first newline char.
  size_t newline_pos = lo_dev_name_p->find('\n');
  if (newline_pos != string::npos)
    lo_dev_name_p->erase(newline_pos);

  return true;
}

bool ExpectVectorsEq(const vector<char>& expected, const vector<char>& actual) {
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

void FillWithData(vector<char>* buffer) {
  size_t input_counter = 0;
  for (vector<char>::iterator it = buffer->begin(); it != buffer->end(); ++it) {
    *it = kRandomString[input_counter];
    input_counter++;
    input_counter %= sizeof(kRandomString);
  }
}

void CreateEmptyExtImageAtPath(const string& path,
                               size_t size,
                               int block_size) {
  EXPECT_EQ(0, System(StringPrintf("dd if=/dev/zero of=%s"
                                   " seek=%zu bs=1 count=1",
                                   path.c_str(), size)));
  EXPECT_EQ(0, System(StringPrintf("mkfs.ext3 -b %d -F %s",
                                   block_size, path.c_str())));
}

void CreateExtImageAtPath(const string& path, vector<string>* out_paths) {
  // create 10MiB sparse file, mounted at a unique location.
  string mount_path;
  CHECK(utils::MakeTempDirectory(kMountPathTemplate, &mount_path));
  ScopedDirRemover mount_path_unlinker(mount_path);

  EXPECT_EQ(0, System(StringPrintf("dd if=/dev/zero of=%s"
                                   " seek=10485759 bs=1 count=1",
                                   path.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mkfs.ext3 -b 4096 -F %s", path.c_str())));
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

void VerifyAllPaths(const string& parent, set<string> expected_paths) {
  FilesystemIterator iter(parent, set<string>());
  ino_t test_ino = 0;
  ino_t testlink_ino = 0;
  while (!iter.IsEnd()) {
    string path = iter.GetFullPath();
    EXPECT_TRUE(expected_paths.find(path) != expected_paths.end()) << path;
    EXPECT_EQ(1, expected_paths.erase(path));
    if (utils::StringHasSuffix(path, "/hi") ||
        utils::StringHasSuffix(path, "/hello") ||
        utils::StringHasSuffix(path, "/test") ||
        utils::StringHasSuffix(path, "/testlink")) {
      EXPECT_TRUE(S_ISREG(iter.GetStat().st_mode));
      if (utils::StringHasSuffix(path, "/test"))
        test_ino = iter.GetStat().st_ino;
      else if (utils::StringHasSuffix(path, "/testlink"))
        testlink_ino = iter.GetStat().st_ino;
    } else if (utils::StringHasSuffix(path, "/some_dir") ||
               utils::StringHasSuffix(path, "/empty_dir") ||
               utils::StringHasSuffix(path, "/mnt") ||
               utils::StringHasSuffix(path, "/lost+found") ||
               parent == path) {
      EXPECT_TRUE(S_ISDIR(iter.GetStat().st_mode));
    } else if (utils::StringHasSuffix(path, "/fifo")) {
      EXPECT_TRUE(S_ISFIFO(iter.GetStat().st_mode));
    } else if (utils::StringHasSuffix(path, "/cdev")) {
      EXPECT_TRUE(S_ISCHR(iter.GetStat().st_mode));
    } else if (utils::StringHasSuffix(path, "/sym")) {
      EXPECT_TRUE(S_ISLNK(iter.GetStat().st_mode));
    } else {
      LOG(INFO) << "got non hardcoded path: " << path;
    }
    iter.Increment();
  }
  EXPECT_EQ(testlink_ino, test_ino);
  EXPECT_NE(0, test_ino);
  EXPECT_FALSE(iter.IsErr());
  EXPECT_TRUE(expected_paths.empty());
  if (!expected_paths.empty()) {
    for (const string& path : expected_paths) {
      LOG(INFO) << "extra path: " << path;
    }
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

static gboolean RunGMainLoopOnTimeout(gpointer user_data) {
  bool* timeout = static_cast<bool*>(user_data);
  *timeout = true;
  return FALSE;  // Remove timeout source
}

void RunGMainLoopUntil(int timeout_msec, base::Callback<bool()> terminate) {
  GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
  GMainContext* context = g_main_context_default();

  bool timeout = false;
  guint source_id = g_timeout_add(
      timeout_msec, RunGMainLoopOnTimeout, &timeout);

  while (!timeout && (terminate.is_null() || !terminate.Run()))
    g_main_context_iteration(context, TRUE);

  g_source_remove(source_id);
  g_main_loop_unref(loop);
}

int RunGMainLoopMaxIterations(int iterations) {
  int result;
  GMainContext* context = g_main_context_default();
  for (result = 0;
       result < iterations && g_main_context_iteration(context, FALSE);
       result++) {}
  return result;
}

GValue* GValueNewString(const char* str) {
  GValue* gval = g_new0(GValue, 1);
  g_value_init(gval, G_TYPE_STRING);
  g_value_set_string(gval, str);
  return gval;
}

void GValueFree(gpointer arg) {
  auto gval = reinterpret_cast<GValue*>(arg);
  g_value_unset(gval);
  g_free(gval);
}

}  // namespace chromeos_update_engine
