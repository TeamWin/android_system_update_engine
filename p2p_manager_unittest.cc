// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/p2p_manager.h"

#include <dirent.h>
#include <fcntl.h>
#include <glib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <attr/xattr.h>  // NOLINT - requires typed defined in unistd.h

#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/strings/stringprintf.h>
#include <chromeos/message_loops/glib_message_loop.h>
#include <chromeos/message_loops/message_loop.h>
#include <chromeos/message_loops/message_loop_utils.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <policy/libpolicy.h>
#include <policy/mock_device_policy.h>

#include "update_engine/fake_clock.h"
#include "update_engine/fake_p2p_manager_configuration.h"
#include "update_engine/prefs.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_manager/fake_update_manager.h"
#include "update_engine/update_manager/mock_policy.h"
#include "update_engine/utils.h"

using base::TimeDelta;
using chromeos::MessageLoop;
using chromeos_update_engine::test_utils::System;
using std::string;
using std::vector;
using std::unique_ptr;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;
using testing::_;

namespace chromeos_update_engine {

// Test fixture that sets up a testing configuration (with e.g. a
// temporary p2p dir) for P2PManager and cleans up when the test is
// done.
class P2PManagerTest : public testing::Test {
 protected:
  P2PManagerTest() : fake_um_(&fake_clock_) {}
  ~P2PManagerTest() override {}

  // Derived from testing::Test.
  void SetUp() override {
    loop_.SetAsCurrent();
    test_conf_ = new FakeP2PManagerConfiguration();

    // Allocate and install a mock policy implementation in the fake Update
    // Manager.  Note that the FakeUpdateManager takes ownership of the policy
    // object.
    mock_policy_ = new chromeos_update_manager::MockPolicy(&fake_clock_);
    fake_um_.set_policy(mock_policy_);

    // Construct the P2P manager under test.
    manager_.reset(P2PManager::Construct(test_conf_, &fake_clock_, &fake_um_,
                                         "cros_au", 3,
                                         TimeDelta::FromDays(5)));
  }

  void TearDown() override {
    EXPECT_EQ(0, chromeos::MessageLoopRunMaxIterations(&loop_, 1));
  }

  // TODO(deymo): Replace this with a FakeMessageLoop. P2PManager uses glib to
  // interact with the p2p-client tool, so we need to run a GlibMessageLoop
  // here.
  chromeos::GlibMessageLoop loop_;

  // The P2PManager::Configuration instance used for testing.
  FakeP2PManagerConfiguration *test_conf_;

  FakeClock fake_clock_;
  chromeos_update_manager::MockPolicy *mock_policy_ = nullptr;
  chromeos_update_manager::FakeUpdateManager fake_um_;

  unique_ptr<P2PManager> manager_;
};


// Check that IsP2PEnabled() polls the policy correctly, with the value not
// changing between calls.
TEST_F(P2PManagerTest, P2PEnabledInitAndNotChanged) {
  EXPECT_CALL(*mock_policy_, P2PEnabled(_, _, _, _));
  EXPECT_CALL(*mock_policy_, P2PEnabledChanged(_, _, _, _, false));

  EXPECT_FALSE(manager_->IsP2PEnabled());
  chromeos::MessageLoopRunMaxIterations(MessageLoop::current(), 100);
  EXPECT_FALSE(manager_->IsP2PEnabled());
}

// Check that IsP2PEnabled() polls the policy correctly, with the value changing
// between calls.
TEST_F(P2PManagerTest, P2PEnabledInitAndChanged) {
  EXPECT_CALL(*mock_policy_, P2PEnabled(_, _, _, _))
      .WillOnce(DoAll(
              SetArgPointee<3>(true),
              Return(chromeos_update_manager::EvalStatus::kSucceeded)));
  EXPECT_CALL(*mock_policy_, P2PEnabledChanged(_, _, _, _, true));
  EXPECT_CALL(*mock_policy_, P2PEnabledChanged(_, _, _, _, false));

  EXPECT_TRUE(manager_->IsP2PEnabled());
  chromeos::MessageLoopRunMaxIterations(MessageLoop::current(), 100);
  EXPECT_FALSE(manager_->IsP2PEnabled());
}

// Check that we keep the $N newest files with the .$EXT.p2p extension.
TEST_F(P2PManagerTest, HousekeepingCountLimit) {
  // Specifically pass 0 for |max_file_age| to allow files of any age. Note that
  // we need to reallocate the test_conf_ member, whose currently aliased object
  // will be freed.
  test_conf_ = new FakeP2PManagerConfiguration();
  manager_.reset(P2PManager::Construct(
      test_conf_, &fake_clock_, &fake_um_, "cros_au", 3,
      TimeDelta() /* max_file_age */));
  EXPECT_EQ(manager_->CountSharedFiles(), 0);

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
  EXPECT_EQ(manager_->CountSharedFiles(), 5);

  EXPECT_TRUE(manager_->PerformHousekeeping());

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
  EXPECT_EQ(manager_->CountSharedFiles(), 3);
}

// Check that we keep files with the .$EXT.p2p extension not older
// than some specificed age (5 days, in this test).
TEST_F(P2PManagerTest, HousekeepingAgeLimit) {
  // We set the cutoff time to be 1 billion seconds (01:46:40 UTC on 9
  // September 2001 - arbitrary number, but constant to avoid test
  // flakiness) since the epoch and then we put two files before that
  // date and three files after.
  time_t cutoff_time = 1000000000;
  TimeDelta age_limit = TimeDelta::FromDays(5);

  // Set the clock just so files with a timestamp before |cutoff_time|
  // will be deleted at housekeeping.
  fake_clock_.SetWallclockTime(base::Time::FromTimeT(cutoff_time) + age_limit);

  // Specifically pass 0 for |num_files_to_keep| to allow files of any age.
  // Note that we need to reallocate the test_conf_ member, whose currently
  // aliased object will be freed.
  test_conf_ = new FakeP2PManagerConfiguration();
  manager_.reset(P2PManager::Construct(
      test_conf_, &fake_clock_, &fake_um_, "cros_au",
      0 /* num_files_to_keep */, age_limit));
  EXPECT_EQ(manager_->CountSharedFiles(), 0);

  // Generate files with different timestamps matching our pattern and generate
  // other files not matching the pattern.
  for (int n = 0; n < 5; n++) {
    base::FilePath file = test_conf_->GetP2PDir().Append(base::StringPrintf(
        "file_%d.cros_au.p2p", n));

    // With five files and aiming for two of them to be before
    // |cutoff_time|, we distribute it like this:
    //
    //  -------- 0 -------- 1 -------- 2 -------- 3 -------- 4 --------
    //                            |
    //                       cutoff_time
    //
    base::Time file_date = base::Time::FromTimeT(cutoff_time) +
      (n - 2)*TimeDelta::FromDays(1) + TimeDelta::FromHours(12);

    // The touch(1) command expects input like this
    // --date="2004-02-27 14:19:13.489392193 +0530"
    base::Time::Exploded exploded;
    file_date.UTCExplode(&exploded);
    string file_date_string = base::StringPrintf(
        "%d-%02d-%02d %02d:%02d:%02d +0000",
        exploded.year, exploded.month, exploded.day_of_month,
        exploded.hour, exploded.minute, exploded.second);

    // Sanity check that we generated the correct string.
    base::Time parsed_time;
    EXPECT_TRUE(base::Time::FromUTCString(file_date_string.c_str(),
                                          &parsed_time));
    EXPECT_EQ(parsed_time, file_date);

    EXPECT_EQ(0, System(base::StringPrintf("touch --date=\"%s\" %s",
                                           file_date_string.c_str(),
                                           file.value().c_str())));

    EXPECT_EQ(0, System(base::StringPrintf(
        "touch --date=\"%s\" %s/file_%d.OTHER.p2p",
        file_date_string.c_str(),
        test_conf_->GetP2PDir().value().c_str(), n)));
  }
  // CountSharedFiles() only counts 'cros_au' files.
  EXPECT_EQ(manager_->CountSharedFiles(), 5);

  EXPECT_TRUE(manager_->PerformHousekeeping());

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
  EXPECT_EQ(manager_->CountSharedFiles(), 3);
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
  if (!test_utils::IsXAttrSupported(base::FilePath("/tmp"))) {
    LOG(WARNING) << "Skipping test because /tmp does not support xattr. "
                 << "Please update your system to support this feature.";
    return;
  }
  const int kP2PTestFileSize = 1000 * 1000;  // 1 MB

  EXPECT_TRUE(manager_->FileShare("foo", kP2PTestFileSize));
  EXPECT_EQ(manager_->FileGetPath("foo"),
            test_conf_->GetP2PDir().Append("foo.cros_au.p2p.tmp"));
  EXPECT_TRUE(CheckP2PFile(test_conf_->GetP2PDir().value(),
                           "foo.cros_au.p2p.tmp", 0, kP2PTestFileSize));

  // Sharing it again - with the same expected size - should return true
  EXPECT_TRUE(manager_->FileShare("foo", kP2PTestFileSize));

  // ... but if we use the wrong size, it should fail
  EXPECT_FALSE(manager_->FileShare("foo", kP2PTestFileSize + 1));
}

// Check that making a shared file visible, does what is expected.
TEST_F(P2PManagerTest, MakeFileVisible) {
  if (!test_utils::IsXAttrSupported(base::FilePath("/tmp"))) {
    LOG(WARNING) << "Skipping test because /tmp does not support xattr. "
                 << "Please update your system to support this feature.";
    return;
  }
  const int kP2PTestFileSize = 1000 * 1000;  // 1 MB

  // First, check that it's not visible.
  manager_->FileShare("foo", kP2PTestFileSize);
  EXPECT_EQ(manager_->FileGetPath("foo"),
            test_conf_->GetP2PDir().Append("foo.cros_au.p2p.tmp"));
  EXPECT_TRUE(CheckP2PFile(test_conf_->GetP2PDir().value(),
                           "foo.cros_au.p2p.tmp", 0, kP2PTestFileSize));
  // Make the file visible and check that it changed its name. Do it
  // twice to check that FileMakeVisible() is idempotent.
  for (int n = 0; n < 2; n++) {
    manager_->FileMakeVisible("foo");
    EXPECT_EQ(manager_->FileGetPath("foo"),
              test_conf_->GetP2PDir().Append("foo.cros_au.p2p"));
    EXPECT_TRUE(CheckP2PFile(test_conf_->GetP2PDir().value(),
                             "foo.cros_au.p2p", 0, kP2PTestFileSize));
  }
}

// Check that we return the right values for existing files in P2P_DIR.
TEST_F(P2PManagerTest, ExistingFiles) {
  if (!test_utils::IsXAttrSupported(base::FilePath("/tmp"))) {
    LOG(WARNING) << "Skipping test because /tmp does not support xattr. "
                 << "Please update your system to support this feature.";
    return;
  }

  bool visible;

  // Check that errors are returned if the file does not exist
  EXPECT_EQ(manager_->FileGetPath("foo"), base::FilePath());
  EXPECT_EQ(manager_->FileGetSize("foo"), -1);
  EXPECT_EQ(manager_->FileGetExpectedSize("foo"), -1);
  EXPECT_FALSE(manager_->FileGetVisible("foo", nullptr));
  // ... then create the file ...
  EXPECT_TRUE(CreateP2PFile(test_conf_->GetP2PDir().value(),
                            "foo.cros_au.p2p", 42, 43));
  // ... and then check that the expected values are returned
  EXPECT_EQ(manager_->FileGetPath("foo"),
            test_conf_->GetP2PDir().Append("foo.cros_au.p2p"));
  EXPECT_EQ(manager_->FileGetSize("foo"), 42);
  EXPECT_EQ(manager_->FileGetExpectedSize("foo"), 43);
  EXPECT_TRUE(manager_->FileGetVisible("foo", &visible));
  EXPECT_TRUE(visible);

  // One more time, this time with a .tmp variant. First ensure it errors out..
  EXPECT_EQ(manager_->FileGetPath("bar"), base::FilePath());
  EXPECT_EQ(manager_->FileGetSize("bar"), -1);
  EXPECT_EQ(manager_->FileGetExpectedSize("bar"), -1);
  EXPECT_FALSE(manager_->FileGetVisible("bar", nullptr));
  // ... then create the file ...
  EXPECT_TRUE(CreateP2PFile(test_conf_->GetP2PDir().value(),
                            "bar.cros_au.p2p.tmp", 44, 45));
  // ... and then check that the expected values are returned
  EXPECT_EQ(manager_->FileGetPath("bar"),
            test_conf_->GetP2PDir().Append("bar.cros_au.p2p.tmp"));
  EXPECT_EQ(manager_->FileGetSize("bar"), 44);
  EXPECT_EQ(manager_->FileGetExpectedSize("bar"), 45);
  EXPECT_TRUE(manager_->FileGetVisible("bar", &visible));
  EXPECT_FALSE(visible);
}

// This is a little bit ugly but short of mocking a 'p2p' service this
// will have to do. E.g. we essentially simulate the various
// behaviours of initctl(8) that we rely on.
TEST_F(P2PManagerTest, StartP2P) {
  // Check that we can start the service
  test_conf_->SetInitctlStartCommand({"true"});
  EXPECT_TRUE(manager_->EnsureP2PRunning());
  test_conf_->SetInitctlStartCommand({"false"});
  EXPECT_FALSE(manager_->EnsureP2PRunning());
  test_conf_->SetInitctlStartCommand({
      "sh", "-c", "echo \"initctl: Job is already running: p2p\" >&2; false"});
  EXPECT_TRUE(manager_->EnsureP2PRunning());
  test_conf_->SetInitctlStartCommand({
      "sh", "-c", "echo something else >&2; false"});
  EXPECT_FALSE(manager_->EnsureP2PRunning());
}

// Same comment as for StartP2P
TEST_F(P2PManagerTest, StopP2P) {
  // Check that we can start the service
  test_conf_->SetInitctlStopCommand({"true"});
  EXPECT_TRUE(manager_->EnsureP2PNotRunning());
  test_conf_->SetInitctlStopCommand({"false"});
  EXPECT_FALSE(manager_->EnsureP2PNotRunning());
  test_conf_->SetInitctlStopCommand({
      "sh", "-c", "echo \"initctl: Unknown instance \" >&2; false"});
  EXPECT_TRUE(manager_->EnsureP2PNotRunning());
  test_conf_->SetInitctlStopCommand({
      "sh", "-c", "echo something else >&2; false"});
  EXPECT_FALSE(manager_->EnsureP2PNotRunning());
}

static void ExpectUrl(const string& expected_url,
                      const string& url) {
  EXPECT_EQ(url, expected_url);
  MessageLoop::current()->BreakLoop();
}

// Like StartP2P, we're mocking the different results that p2p-client
// can return. It's not pretty but it works.
TEST_F(P2PManagerTest, LookupURL) {
  // Emulate p2p-client returning valid URL with "fooX", 42 and "cros_au"
  // being propagated in the right places.
  test_conf_->SetP2PClientCommand({
      "echo", "http://1.2.3.4/{file_id}_{minsize}"});
  manager_->LookupUrlForFile("fooX", 42, TimeDelta(),
                             base::Bind(ExpectUrl,
                                        "http://1.2.3.4/fooX.cros_au_42"));
  loop_.Run();

  // Emulate p2p-client returning invalid URL.
  test_conf_->SetP2PClientCommand({"echo", "not_a_valid_url"});
  manager_->LookupUrlForFile("foobar", 42, TimeDelta(),
                             base::Bind(ExpectUrl, ""));
  loop_.Run();

  // Emulate p2p-client conveying failure.
  test_conf_->SetP2PClientCommand({"false"});
  manager_->LookupUrlForFile("foobar", 42, TimeDelta(),
                             base::Bind(ExpectUrl, ""));
  loop_.Run();

  // Emulate p2p-client not existing.
  test_conf_->SetP2PClientCommand({"/path/to/non/existent/helper/program"});
  manager_->LookupUrlForFile("foobar", 42,
                             TimeDelta(),
                             base::Bind(ExpectUrl, ""));
  loop_.Run();

  // Emulate p2p-client crashing.
  test_conf_->SetP2PClientCommand({"sh", "-c", "kill -SEGV $$"});
  manager_->LookupUrlForFile("foobar", 42, TimeDelta(),
                             base::Bind(ExpectUrl, ""));
  loop_.Run();

  // Emulate p2p-client exceeding its timeout.
  test_conf_->SetP2PClientCommand({
      "sh", "-c", "echo http://1.2.3.4/; sleep 2"});
  manager_->LookupUrlForFile("foobar", 42, TimeDelta::FromMilliseconds(500),
                             base::Bind(ExpectUrl, ""));
  loop_.Run();
}

}  // namespace chromeos_update_engine
