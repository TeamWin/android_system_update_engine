// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <attr/xattr.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/stringprintf.h"

#include "update_engine/p2p_manager.h"
#include "update_engine/fake_p2p_manager_configuration.h"
#include "update_engine/prefs.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;
using base::TimeDelta;

namespace chromeos_update_engine {

// Test fixture that sets up a testing configuration (with e.g. a
// temporary p2p dir) for P2PManager and cleans up when the test is
// done.
class P2PManagerTest : public testing::Test {
protected:
  P2PManagerTest() {}
  virtual ~P2PManagerTest() {}

  // Derived from testing::Test.
  virtual void SetUp() {
    test_conf_ = new FakeP2PManagerConfiguration();
  }
  virtual void TearDown() {}

  // The P2PManager::Configuration instance used for testing.
  FakeP2PManagerConfiguration *test_conf_;
};


// Check that result of IsP2PEnabled() correspond to the
// kPrefsP2PEnabled state variable.
TEST_F(P2PManagerTest, P2PEnabled) {
  string temp_dir;
  Prefs prefs;
  EXPECT_TRUE(utils::MakeTempDirectory("/tmp/PayloadStateP2PTests.XXXXXX",
                                       &temp_dir));
  prefs.Init(FilePath(temp_dir));

  scoped_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       &prefs, "cros_au", 3));
  EXPECT_FALSE(manager->IsP2PEnabled());
  prefs.SetBoolean(kPrefsP2PEnabled, true);
  EXPECT_TRUE(manager->IsP2PEnabled());
  prefs.SetBoolean(kPrefsP2PEnabled, false);
  EXPECT_FALSE(manager->IsP2PEnabled());

  EXPECT_TRUE(utils::RecursiveUnlinkDir(temp_dir));
}

// Check that we keep the $N newest files with the .$EXT.p2p extension.
TEST_F(P2PManagerTest, HouseKeeping) {
  scoped_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       NULL, "cros_au", 3));
  EXPECT_EQ(manager->CountSharedFiles(), 0);

  // Generate files both matching our pattern and not matching them.
  for (int n = 0; n < 5; n++) {
    EXPECT_EQ(0, System(StringPrintf("touch %s/file_%d.cros_au.p2p",
                                     test_conf_->GetP2PDir().value().c_str(),
                                     n)));

    EXPECT_EQ(0, System(StringPrintf("touch %s/file_%d.OTHER.p2p",
                                     test_conf_->GetP2PDir().value().c_str(),
                                     n)));

    // Sleep one micro-second to ensure that the files all have
    // different timestamps (time resolution for ext4 is one
    // nano-second so sleeping a single usec is more than enough).
    g_usleep(1);
  }
  // CountSharedFiles() only counts 'cros_au' files.
  EXPECT_EQ(manager->CountSharedFiles(), 5);

  EXPECT_TRUE(manager->PerformHousekeeping());

  // At this point - after HouseKeeping - we should only have
  // eight files left.
  for (int n = 0; n < 5; n++) {
    string file_name;
    bool expect;

    expect = (n >= 2);
    file_name = StringPrintf("%s/file_%d.cros_au.p2p",
                             test_conf_->GetP2PDir().value().c_str(), n);
    EXPECT_EQ(!!g_file_test(file_name.c_str(), G_FILE_TEST_EXISTS), expect);

    file_name = StringPrintf("%s/file_%d.OTHER.p2p",
                             test_conf_->GetP2PDir().value().c_str(), n);
    EXPECT_TRUE(g_file_test(file_name.c_str(), G_FILE_TEST_EXISTS));
  }
  // CountSharedFiles() only counts 'cros_au' files.
  EXPECT_EQ(manager->CountSharedFiles(), 3);
}


// TODO(zeuthen): Some builders do not support fallocate(2) or xattrs
// in the tmp directories where the code runs so comment out these
// tests for now. See http://crbug.com/281496
#if 0

static bool CheckP2PFile(const string& p2p_dir, const string& file_name,
                         ssize_t expected_size, ssize_t expected_size_xattr) {
  string path = p2p_dir + "/" + file_name;
  struct stat statbuf;
  char ea_value[64] = { 0 };
  ssize_t ea_size;

  if (stat(path.c_str(), &statbuf) != 0) {
    LOG(ERROR) << "File " << path << " does not exist";
    return false;
  }

  if (expected_size != 0) {
    if (statbuf.st_size != expected_size) {
      LOG(ERROR) << "Expected size " << expected_size
                 << " but size was " << statbuf.st_size;
      return false;
    }
  }

  if (expected_size_xattr == 0) {
    ea_size = getxattr(path.c_str(), "user.cros-p2p-filesize",
                       &ea_value, sizeof ea_value - 1);
    if (ea_size == -1 && errno == ENOATTR) {
      // This is valid behavior as we support files without the xattr set.
    } else {
      PLOG(ERROR) << "getxattr() didn't fail with ENOATTR as expected, "
                  << "ea_size=" << ea_size << ", errno=" << errno;
      return false;
    }
  } else {
    ea_size = getxattr(path.c_str(), "user.cros-p2p-filesize",
                       &ea_value, sizeof ea_value - 1);
    if (ea_size < 0) {
      LOG(ERROR) << "Error getting xattr attribute";
      return false;
    }
    char* endp = NULL;
    long long int val = strtoll(ea_value, &endp, 0);
    if (endp == NULL || *endp != '\0') {
      LOG(ERROR) << "Error parsing xattr '" << ea_value
                 << "' as an integer";
      return false;
    }
    if (val != expected_size_xattr) {
      LOG(ERROR) << "Expected xattr size " << expected_size_xattr
                 << " but size was " << val;
      return false;
    }
  }

  return true;
}

static bool CreateP2PFile(string p2p_dir, string file_name,
                          size_t size, size_t size_xattr) {
  string path = p2p_dir + "/" + file_name;

  int fd = open(path.c_str(), O_CREAT|O_RDWR, 0644);
  if (fd == -1) {
    PLOG(ERROR) << "Error creating file with path " << path;
    return false;
  }
  if (ftruncate(fd, size) != 0) {
    PLOG(ERROR) << "Error truncating " << path << " to size " << size;
    close(fd);
    return false;
  }

  if (size_xattr != 0) {
    string decimal_size = StringPrintf("%zu", size_xattr);
    if (fsetxattr(fd, "user.cros-p2p-filesize",
                  decimal_size.c_str(), decimal_size.size(), 0) != 0) {
      PLOG(ERROR) << "Error setting xattr on " << path;
      close(fd);
      return false;
    }
  }

  close(fd);
  return true;
}

// Check that sharing a *new* file works.
TEST_F(P2PManagerTest, ShareFile) {
  scoped_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       NULL, "cros_au", 3));
  EXPECT_TRUE(manager->FileShare("foo", 10 * 1000 * 1000));
  EXPECT_EQ(manager->FileGetPath("foo"),
            test_conf_->GetP2PDir().Append("foo.cros_au.p2p.tmp"));
  EXPECT_TRUE(CheckP2PFile(test_conf_->GetP2PDir().value(),
                           "foo.cros_au.p2p.tmp", 0, 10 * 1000 * 1000));

  // Sharing it again - with the same expected size - should return true
  EXPECT_TRUE(manager->FileShare("foo", 10 * 1000 * 1000));

  // ... but if we use the wrong size, it should fail
  EXPECT_FALSE(manager->FileShare("foo", 10 * 1000 * 1000 + 1));
}

// Check that making a shared file visible, does what is expected.
TEST_F(P2PManagerTest, MakeFileVisible) {
  scoped_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       NULL, "cros_au", 3));
  // First, check that it's not visible.
  manager->FileShare("foo", 10*1000*1000);
  EXPECT_EQ(manager->FileGetPath("foo"),
            test_conf_->GetP2PDir().Append("foo.cros_au.p2p.tmp"));
  EXPECT_TRUE(CheckP2PFile(test_conf_->GetP2PDir().value(),
                           "foo.cros_au.p2p.tmp", 0, 10 * 1000 * 1000));
  // Make the file visible and check that it changed its name. Do it
  // twice to check that FileMakeVisible() is idempotent.
  for (int n = 0; n < 2; n++) {
    manager->FileMakeVisible("foo");
    EXPECT_EQ(manager->FileGetPath("foo"),
              test_conf_->GetP2PDir().Append("foo.cros_au.p2p"));
    EXPECT_TRUE(CheckP2PFile(test_conf_->GetP2PDir().value(),
                             "foo.cros_au.p2p", 0, 10 * 1000 * 1000));
  }
}

// Check that we return the right values for existing files in P2P_DIR.
TEST_F(P2PManagerTest, ExistingFiles) {
  scoped_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       NULL, "cros_au", 3));
  bool visible;

  // Check that errors are returned if the file does not exist
  EXPECT_EQ(manager->FileGetPath("foo"), FilePath());
  EXPECT_EQ(manager->FileGetSize("foo"), -1);
  EXPECT_EQ(manager->FileGetExpectedSize("foo"), -1);
  EXPECT_FALSE(manager->FileGetVisible("foo", NULL));
  // ... then create the file ...
  EXPECT_TRUE(CreateP2PFile(test_conf_->GetP2PDir().value(),
                            "foo.cros_au.p2p", 42, 43));
  // ... and then check that the expected values are returned
  EXPECT_EQ(manager->FileGetPath("foo"),
            test_conf_->GetP2PDir().Append("foo.cros_au.p2p"));
  EXPECT_EQ(manager->FileGetSize("foo"), 42);
  EXPECT_EQ(manager->FileGetExpectedSize("foo"), 43);
  EXPECT_TRUE(manager->FileGetVisible("foo", &visible));
  EXPECT_TRUE(visible);

  // One more time, this time with a .tmp variant. First ensure it errors out..
  EXPECT_EQ(manager->FileGetPath("bar"), FilePath());
  EXPECT_EQ(manager->FileGetSize("bar"), -1);
  EXPECT_EQ(manager->FileGetExpectedSize("bar"), -1);
  EXPECT_FALSE(manager->FileGetVisible("bar", NULL));
  // ... then create the file ...
  EXPECT_TRUE(CreateP2PFile(test_conf_->GetP2PDir().value(),
                            "bar.cros_au.p2p.tmp", 44, 45));
  // ... and then check that the expected values are returned
  EXPECT_EQ(manager->FileGetPath("bar"),
            test_conf_->GetP2PDir().Append("bar.cros_au.p2p.tmp"));
  EXPECT_EQ(manager->FileGetSize("bar"), 44);
  EXPECT_EQ(manager->FileGetExpectedSize("bar"), 45);
  EXPECT_TRUE(manager->FileGetVisible("bar", &visible));
  EXPECT_FALSE(visible);
}

#endif // http://crbug.com/281496

// This is a little bit ugly but short of mocking a 'p2p' service this
// will have to do. E.g. we essentially simulate the various
// behaviours of initctl(8) that we rely on.
TEST_F(P2PManagerTest, StartP2P) {
  scoped_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       NULL, "cros_au", 3));

  // Check that we can start the service
  test_conf_->SetInitctlStartCommandLine("true");
  EXPECT_TRUE(manager->EnsureP2PRunning());
  test_conf_->SetInitctlStartCommandLine("false");
  EXPECT_FALSE(manager->EnsureP2PRunning());
  test_conf_->SetInitctlStartCommandLine(
      "sh -c 'echo \"initctl: Job is already running: p2p\" >&2; false'");
  EXPECT_TRUE(manager->EnsureP2PRunning());
  test_conf_->SetInitctlStartCommandLine(
      "sh -c 'echo something else >&2; false'");
  EXPECT_FALSE(manager->EnsureP2PRunning());
}

// Same comment as for StartP2P
TEST_F(P2PManagerTest, StopP2P) {
  scoped_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       NULL, "cros_au", 3));

  // Check that we can start the service
  test_conf_->SetInitctlStopCommandLine("true");
  EXPECT_TRUE(manager->EnsureP2PNotRunning());
  test_conf_->SetInitctlStopCommandLine("false");
  EXPECT_FALSE(manager->EnsureP2PNotRunning());
  test_conf_->SetInitctlStopCommandLine(
      "sh -c 'echo \"initctl: Unknown instance \" >&2; false'");
  EXPECT_TRUE(manager->EnsureP2PNotRunning());
  test_conf_->SetInitctlStopCommandLine(
      "sh -c 'echo something else >&2; false'");
  EXPECT_FALSE(manager->EnsureP2PNotRunning());
}

static void ExpectUrl(const string& expected_url,
                      GMainLoop* loop,
                      const string& url) {
  EXPECT_EQ(url, expected_url);
  g_main_loop_quit(loop);
}

// Like StartP2P, we're mocking the different results that p2p-client
// can return. It's not pretty but it works.
TEST_F(P2PManagerTest, LookupURL) {
  scoped_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       NULL, "cros_au", 3));
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);

  // Emulate p2p-client returning valid URL with "fooX", 42 and "cros_au"
  // being propagated in the right places.
  test_conf_->SetP2PClientCommandLine("echo 'http://1.2.3.4/%s_%zu'");
  manager->LookupUrlForFile("fooX", 42, TimeDelta(),
                            base::Bind(ExpectUrl,
                                       "http://1.2.3.4/fooX.cros_au_42", loop));
  g_main_loop_run(loop);

  // Emulate p2p-client returning invalid URL.
  test_conf_->SetP2PClientCommandLine("echo 'not_a_valid_url'");
  manager->LookupUrlForFile("foobar", 42, TimeDelta(),
                            base::Bind(ExpectUrl, "", loop));
  g_main_loop_run(loop);

  // Emulate p2p-client conveying failure.
  test_conf_->SetP2PClientCommandLine("false");
  manager->LookupUrlForFile("foobar", 42, TimeDelta(),
                            base::Bind(ExpectUrl, "", loop));
  g_main_loop_run(loop);

  // Emulate p2p-client not existing.
  test_conf_->SetP2PClientCommandLine("/path/to/non/existent/helper/program");
  manager->LookupUrlForFile("foobar", 42,
                            TimeDelta(),
                            base::Bind(ExpectUrl, "", loop));
  g_main_loop_run(loop);

  // Emulate p2p-client crashing.
  test_conf_->SetP2PClientCommandLine("sh -c 'kill -SEGV $$'");
  manager->LookupUrlForFile("foobar", 42, TimeDelta(),
                            base::Bind(ExpectUrl, "", loop));
  g_main_loop_run(loop);

  // Emulate p2p-client exceeding its timeout.
  test_conf_->SetP2PClientCommandLine("sh -c 'echo http://1.2.3.4/; sleep 2'");
  manager->LookupUrlForFile("foobar", 42, TimeDelta::FromMilliseconds(500),
                            base::Bind(ExpectUrl, "", loop));
  g_main_loop_run(loop);

  g_main_loop_unref(loop);
}

} // namespace chromeos_update_engine
