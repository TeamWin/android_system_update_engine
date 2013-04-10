// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/postinstall_runner_action.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <gtest/gtest.h>

#include "update_engine/constants.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
gboolean StartProcessorInRunLoop(gpointer data) {
  ActionProcessor *processor = reinterpret_cast<ActionProcessor*>(data);
  processor->StartProcessing();
  return FALSE;
}
}  // namespace

class PostinstallRunnerActionTest : public ::testing::Test {
 public:
  // DoTest with various combinations of do_losetup, err_code and
  // powerwash_required.
  void DoTest(bool do_losetup, int err_code, bool powerwash_required);
};

class PostinstActionProcessorDelegate : public ActionProcessorDelegate {
 public:
  PostinstActionProcessorDelegate()
      : loop_(NULL),
        code_(kActionCodeError),
        code_set_(false) {}
  void ProcessingDone(const ActionProcessor* processor,
                      ActionExitCode code) {
    ASSERT_TRUE(loop_);
    g_main_loop_quit(loop_);
  }
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ActionExitCode code) {
    if (action->Type() == PostinstallRunnerAction::StaticType()) {
      code_ = code;
      code_set_ = true;
    }
  }
  GMainLoop* loop_;
  ActionExitCode code_;
  bool code_set_;
};

TEST_F(PostinstallRunnerActionTest, RunAsRootSimpleTest) {
  ASSERT_EQ(0, getuid());
  DoTest(true, 0, false);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootPowerwashRequiredTest) {
  ASSERT_EQ(0, getuid());
  DoTest(true, 0, true);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootCantMountTest) {
  ASSERT_EQ(0, getuid());
  DoTest(false, 0, true);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootErrScriptTest) {
  ASSERT_EQ(0, getuid());
  DoTest(true, 1, false);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootFirmwareBErrScriptTest) {
  ASSERT_EQ(0, getuid());
  DoTest(true, 3, false);
}

void PostinstallRunnerActionTest::DoTest(
    bool do_losetup,
    int err_code,
    bool powerwash_required) {
  ASSERT_EQ(0, getuid()) << "Run me as root. Ideally don't run other tests "
                         << "as root, tho.";
  // True if the post-install action is expected to succeed.
  bool should_succeed = do_losetup && !err_code;

  const string mountpoint(string(kStatefulPartition) +
                          "/au_destination");

  string cwd;
  {
    vector<char> buf(1000);
    ASSERT_EQ(&buf[0], getcwd(&buf[0], buf.size()));
    cwd = string(&buf[0], strlen(&buf[0]));
  }

  // create the au destination, if it doesn't exist
  ASSERT_EQ(0, System(string("mkdir -p ") + mountpoint));

  // create 10MiB sparse file
  ASSERT_EQ(0, system("dd if=/dev/zero of=image.dat seek=10485759 bs=1 "
                      "count=1"));

  // format it as ext2
  ASSERT_EQ(0, system("mkfs.ext2 -F image.dat"));

  // mount it
  ASSERT_EQ(0, System(string("mount -o loop image.dat ") + mountpoint));

  // put a postinst script in
  string script = StringPrintf("#!/bin/bash\n"
                               "mount | grep au_postint_mount | grep ext2\n"
                               "if [ $? -eq 0 ]; then\n"
                               "  touch %s/postinst_called\n"
                               "fi\n",
                               cwd.c_str());
  if (err_code) {
    script = StringPrintf("#!/bin/bash\nexit %d", err_code);
  }
  ASSERT_TRUE(WriteFileString(mountpoint + "/postinst", script));
  ASSERT_EQ(0, System(string("chmod a+x ") + mountpoint + "/postinst"));

  ASSERT_TRUE(utils::UnmountFilesystem(mountpoint));

  ASSERT_EQ(0, System(string("rm -f ") + cwd + "/postinst_called"));

  // get a loop device we can use for the install device
  string dev = "/dev/null";

  scoped_ptr<ScopedLoopbackDeviceBinder> loop_releaser;
  if (do_losetup) {
    loop_releaser.reset(new ScopedLoopbackDeviceBinder(cwd + "/image.dat",
                                                       &dev));
  }

  ActionProcessor processor;
  ObjectFeederAction<InstallPlan> feeder_action;
  InstallPlan install_plan;
  install_plan.install_path = dev;
  install_plan.powerwash_required = powerwash_required;
  feeder_action.set_obj(install_plan);
  PostinstallRunnerAction runner_action;
  BondActions(&feeder_action, &runner_action);
  ObjectCollectorAction<InstallPlan> collector_action;
  BondActions(&runner_action, &collector_action);
  PostinstActionProcessorDelegate delegate;
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&runner_action);
  processor.EnqueueAction(&collector_action);
  processor.set_delegate(&delegate);

  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  delegate.loop_ = loop;
  g_timeout_add(0, &StartProcessorInRunLoop, &processor);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  ASSERT_FALSE(processor.IsRunning());

  EXPECT_TRUE(delegate.code_set_);
  EXPECT_EQ(should_succeed, delegate.code_ == kActionCodeSuccess);
  EXPECT_EQ(should_succeed, !collector_action.object().install_path.empty());
  if (should_succeed)
    EXPECT_TRUE(install_plan == collector_action.object());

  const FilePath kPowerwashMarkerPath(kPowerwashMarkerFile);
  string actual_cmd;
  if (should_succeed && powerwash_required) {
    EXPECT_TRUE(file_util::ReadFileToString(kPowerwashMarkerPath, &actual_cmd));
    EXPECT_EQ(kPowerwashCommand, actual_cmd);
  } else {
    EXPECT_FALSE(
        file_util::ReadFileToString(kPowerwashMarkerPath, &actual_cmd));
  }

  if (err_code == 2)
    EXPECT_EQ(kActionCodePostinstallBootedFromFirmwareB, delegate.code_);

  struct stat stbuf;
  int rc = lstat((string(cwd) + "/postinst_called").c_str(), &stbuf);
  if (should_succeed)
    ASSERT_EQ(0, rc);
  else
    ASSERT_LT(rc, 0);

  if (do_losetup) {
    loop_releaser.reset(NULL);
  }
  ASSERT_EQ(0, System(string("rm -f ") + cwd + "/postinst_called"));
  ASSERT_EQ(0, System(string("rm -f ") + cwd + "/image.dat"));
  utils::DeletePowerwashMarkerFile();
}

// Death tests don't seem to be working on Hardy
TEST_F(PostinstallRunnerActionTest, DISABLED_RunAsRootDeathTest) {
  ASSERT_EQ(0, getuid());
  PostinstallRunnerAction runner_action;
  ASSERT_DEATH({ runner_action.TerminateProcessing(); },
               "postinstall_runner_action.h:.*] Check failed");
}

}  // namespace chromeos_update_engine
