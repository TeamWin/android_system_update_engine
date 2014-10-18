// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <attr/xattr.h>  // NOLINT - requires typed defined in unistd.h

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "base/bind.h"
#include "base/callback.h"
#include <base/strings/stringprintf.h>
#include <policy/libpolicy.h>
#include <policy/mock_device_policy.h>

#include "update_engine/fake_p2p_manager_configuration.h"
#include "update_engine/p2p_manager.h"
#include "update_engine/prefs.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::unique_ptr;
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


// Check that IsP2PEnabled() returns false if neither the crosh flag
// nor the Enterprise Policy indicates p2p is to be used.
TEST_F(P2PManagerTest, P2PEnabledNeitherCroshFlagNotEnterpriseSetting) {
  string temp_dir;
  Prefs prefs;
  EXPECT_TRUE(utils::MakeTempDirectory("PayloadStateP2PTests.XXXXXX",
                                       &temp_dir));
  prefs.Init(base::FilePath(temp_dir));

  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       &prefs, "cros_au", 3));
  EXPECT_FALSE(manager->IsP2PEnabled());

  EXPECT_TRUE(utils::RecursiveUnlinkDir(temp_dir));
}

// Check that IsP2PEnabled() corresponds to value of the crosh flag
// when Enterprise Policy is not set.
TEST_F(P2PManagerTest, P2PEnabledCroshFlag) {
  string temp_dir;
  Prefs prefs;
  EXPECT_TRUE(utils::MakeTempDirectory("PayloadStateP2PTests.XXXXXX",
                                       &temp_dir));
  prefs.Init(base::FilePath(temp_dir));

  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       &prefs, "cros_au", 3));
  EXPECT_FALSE(manager->IsP2PEnabled());
  prefs.SetBoolean(kPrefsP2PEnabled, true);
  EXPECT_TRUE(manager->IsP2PEnabled());
  prefs.SetBoolean(kPrefsP2PEnabled, false);
  EXPECT_FALSE(manager->IsP2PEnabled());

  EXPECT_TRUE(utils::RecursiveUnlinkDir(temp_dir));
}

// Check that IsP2PEnabled() always returns true if Enterprise Policy
// indicates that p2p is to be used.
TEST_F(P2PManagerTest, P2PEnabledEnterpriseSettingTrue) {
  string temp_dir;
  Prefs prefs;
  EXPECT_TRUE(utils::MakeTempDirectory("PayloadStateP2PTests.XXXXXX",
                                       &temp_dir));
  prefs.Init(base::FilePath(temp_dir));

  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       &prefs, "cros_au", 3));
  unique_ptr<policy::MockDevicePolicy> device_policy(
      new policy::MockDevicePolicy());
  EXPECT_CALL(*device_policy, GetAuP2PEnabled(testing::_)).WillRepeatedly(
      DoAll(testing::SetArgumentPointee<0>(true),
            testing::Return(true)));
  manager->SetDevicePolicy(device_policy.get());
  EXPECT_TRUE(manager->IsP2PEnabled());

  // Should still return true even if crosh flag says otherwise.
  prefs.SetBoolean(kPrefsP2PEnabled, false);
  EXPECT_TRUE(manager->IsP2PEnabled());

  EXPECT_TRUE(utils::RecursiveUnlinkDir(temp_dir));
}

// Check that IsP2PEnabled() corresponds to the value of the crosh
// flag if Enterprise Policy indicates that p2p is not to be used.
TEST_F(P2PManagerTest, P2PEnabledEnterpriseSettingFalse) {
  string temp_dir;
  Prefs prefs;
  EXPECT_TRUE(utils::MakeTempDirectory("PayloadStateP2PTests.XXXXXX",
                                       &temp_dir));
  prefs.Init(base::FilePath(temp_dir));

  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       &prefs, "cros_au", 3));
  unique_ptr<policy::MockDevicePolicy> device_policy(
      new policy::MockDevicePolicy());
  EXPECT_CALL(*device_policy, GetAuP2PEnabled(testing::_)).WillRepeatedly(
      DoAll(testing::SetArgumentPointee<0>(false),
            testing::Return(true)));
  manager->SetDevicePolicy(device_policy.get());
  EXPECT_FALSE(manager->IsP2PEnabled());

  prefs.SetBoolean(kPrefsP2PEnabled, true);
  EXPECT_TRUE(manager->IsP2PEnabled());
  prefs.SetBoolean(kPrefsP2PEnabled, false);
  EXPECT_FALSE(manager->IsP2PEnabled());

  EXPECT_TRUE(utils::RecursiveUnlinkDir(temp_dir));
}

// Check that IsP2PEnabled() returns TRUE if
// - The crosh flag is not set.
// - Enterprise Policy is not set.
// - Device is Enterprise Enrolled.
TEST_F(P2PManagerTest, P2PEnabledEnterpriseEnrolledDevicesDefaultToEnabled) {
  string temp_dir;
  Prefs prefs;
  EXPECT_TRUE(utils::MakeTempDirectory("PayloadStateP2PTests.XXXXXX",
                                       &temp_dir));
  prefs.Init(base::FilePath(temp_dir));

  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       &prefs, "cros_au", 3));
  unique_ptr<policy::MockDevicePolicy> device_policy(
      new policy::MockDevicePolicy());
  // We return an empty owner as this is an enterprise.
  EXPECT_CALL(*device_policy, GetOwner(testing::_)).WillRepeatedly(
      DoAll(testing::SetArgumentPointee<0>(std::string("")),
            testing::Return(true)));
  manager->SetDevicePolicy(device_policy.get());
  EXPECT_TRUE(manager->IsP2PEnabled());

  EXPECT_TRUE(utils::RecursiveUnlinkDir(temp_dir));
}

// Check that IsP2PEnabled() returns FALSE if
// - The crosh flag is not set.
// - Enterprise Policy is set to FALSE.
// - Device is Enterprise Enrolled.
TEST_F(P2PManagerTest, P2PEnabledEnterpriseEnrolledDevicesOverrideDefault) {
  string temp_dir;
  Prefs prefs;
  EXPECT_TRUE(utils::MakeTempDirectory("PayloadStateP2PTests.XXXXXX",
                                       &temp_dir));
  prefs.Init(base::FilePath(temp_dir));

  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       &prefs, "cros_au", 3));
  unique_ptr<policy::MockDevicePolicy> device_policy(
      new policy::MockDevicePolicy());
  // We return an empty owner as this is an enterprise.
  EXPECT_CALL(*device_policy, GetOwner(testing::_)).WillRepeatedly(
      DoAll(testing::SetArgumentPointee<0>(std::string("")),
            testing::Return(true)));
  // Make Enterprise Policy indicate p2p is not enabled.
  EXPECT_CALL(*device_policy, GetAuP2PEnabled(testing::_)).WillRepeatedly(
      DoAll(testing::SetArgumentPointee<0>(false),
            testing::Return(true)));
  manager->SetDevicePolicy(device_policy.get());
  EXPECT_FALSE(manager->IsP2PEnabled());

  EXPECT_TRUE(utils::RecursiveUnlinkDir(temp_dir));
}

// Check that we keep the $N newest files with the .$EXT.p2p extension.
TEST_F(P2PManagerTest, Housekeeping) {
  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       nullptr, "cros_au", 3));
  EXPECT_EQ(manager->CountSharedFiles(), 0);

  // Generate files with different timestamps matching our pattern and generate
  // other files not matching the pattern.
  double last_timestamp = -1;
  for (int n = 0; n < 5; n++) {
    double current_timestamp;
    do {
      base::FilePath file = test_conf_->GetP2PDir().Append(base::StringPrintf(
          "file_%d.cros_au.p2p", n));
      EXPECT_EQ(0, System(base::StringPrintf("touch %s",
                                             file.value().c_str())));

      // Check that the current timestamp on the file is different from the
      // previous generated file. This timestamp depends on the file system
      // time resolution, for example, ext2/ext3 have a time resolution of one
      // second while ext4 has a resolution of one nanosecond. If the assigned
      // timestamp is the same, we introduce a bigger sleep and call touch
      // again.
      struct stat statbuf;
      EXPECT_EQ(stat(file.value().c_str(), &statbuf), 0);
      current_timestamp = utils::TimeFromStructTimespec(&statbuf.st_ctim)
          .ToDoubleT();
      if (current_timestamp == last_timestamp)
        sleep(1);
    } while (current_timestamp == last_timestamp);
    last_timestamp = current_timestamp;

    EXPECT_EQ(0, System(base::StringPrintf(
        "touch %s/file_%d.OTHER.p2p",
        test_conf_->GetP2PDir().value().c_str(), n)));

    // A sleep of one micro-second is enough to have a different "Change" time
    // on the file on newer file systems.
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
    file_name = base::StringPrintf(
        "%s/file_%d.cros_au.p2p",
         test_conf_->GetP2PDir().value().c_str(), n);
    EXPECT_EQ(!!g_file_test(file_name.c_str(), G_FILE_TEST_EXISTS), expect);

    file_name = base::StringPrintf(
        "%s/file_%d.OTHER.p2p",
        test_conf_->GetP2PDir().value().c_str(), n);
    EXPECT_TRUE(g_file_test(file_name.c_str(), G_FILE_TEST_EXISTS));
  }
  // CountSharedFiles() only counts 'cros_au' files.
  EXPECT_EQ(manager->CountSharedFiles(), 3);
}

static bool CheckP2PFile(const string& p2p_dir, const string& file_name,
                         ssize_t expected_size, ssize_t expected_size_xattr) {
  string path = p2p_dir + "/" + file_name;
  char ea_value[64] = { 0 };
  ssize_t ea_size;

  off_t p2p_size = utils::FileSize(path);
  if (p2p_size < 0) {
    LOG(ERROR) << "File " << path << " does not exist";
    return false;
  }

  if (expected_size != 0) {
    if (p2p_size != expected_size) {
      LOG(ERROR) << "Expected size " << expected_size
                 << " but size was " << p2p_size;
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
    char* endp = nullptr;
    long long int val = strtoll(ea_value, &endp, 0);  // NOLINT(runtime/int)
    if (endp == nullptr || *endp != '\0') {
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
    string decimal_size = base::StringPrintf("%zu", size_xattr);
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
  if (!utils::IsXAttrSupported(base::FilePath("/tmp"))) {
    LOG(WARNING) << "Skipping test because /tmp does not support xattr. "
                 << "Please update your system to support this feature.";
    return;
  }

  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       nullptr, "cros_au", 3));
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
  if (!utils::IsXAttrSupported(base::FilePath("/tmp"))) {
    LOG(WARNING) << "Skipping test because /tmp does not support xattr. "
                 << "Please update your system to support this feature.";
    return;
  }

  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       nullptr, "cros_au", 3));
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
  if (!utils::IsXAttrSupported(base::FilePath("/tmp"))) {
    LOG(WARNING) << "Skipping test because /tmp does not support xattr. "
                 << "Please update your system to support this feature.";
    return;
  }

  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       nullptr, "cros_au", 3));
  bool visible;

  // Check that errors are returned if the file does not exist
  EXPECT_EQ(manager->FileGetPath("foo"), base::FilePath());
  EXPECT_EQ(manager->FileGetSize("foo"), -1);
  EXPECT_EQ(manager->FileGetExpectedSize("foo"), -1);
  EXPECT_FALSE(manager->FileGetVisible("foo", nullptr));
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
  EXPECT_EQ(manager->FileGetPath("bar"), base::FilePath());
  EXPECT_EQ(manager->FileGetSize("bar"), -1);
  EXPECT_EQ(manager->FileGetExpectedSize("bar"), -1);
  EXPECT_FALSE(manager->FileGetVisible("bar", nullptr));
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

// This is a little bit ugly but short of mocking a 'p2p' service this
// will have to do. E.g. we essentially simulate the various
// behaviours of initctl(8) that we rely on.
TEST_F(P2PManagerTest, StartP2P) {
  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       nullptr, "cros_au", 3));

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
  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       nullptr, "cros_au", 3));

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
  unique_ptr<P2PManager> manager(P2PManager::Construct(test_conf_,
                                                       nullptr, "cros_au", 3));
  GMainLoop *loop = g_main_loop_new(nullptr, FALSE);

  // Emulate p2p-client returning valid URL with "fooX", 42 and "cros_au"
  // being propagated in the right places.
  test_conf_->SetP2PClientCommandLine(
      "echo 'http://1.2.3.4/{file_id}_{minsize}'");
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

}  // namespace chromeos_update_engine
