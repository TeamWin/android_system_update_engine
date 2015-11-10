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

#include "update_engine/payload_consumer/postinstall_runner_action.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/bind_lambda.h>
#include <brillo/message_loops/base_message_loop.h>
#include <brillo/message_loops/message_loop_utils.h>
#include <gtest/gtest.h>

#include "update_engine/common/constants.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/fake_system_state.h"

using brillo::MessageLoop;
using chromeos_update_engine::test_utils::System;
using chromeos_update_engine::test_utils::WriteFileString;
using std::string;
using std::unique_ptr;
using std::vector;

namespace chromeos_update_engine {

class PostinstallRunnerActionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_.SetAsCurrent();
    async_signal_handler_.Init();
    subprocess_.Init(&async_signal_handler_);
  }

  // DoTest with various combinations of do_losetup, err_code and
  // powerwash_required.
  void DoTest(bool do_losetup, int err_code, bool powerwash_required);

 protected:
  static const char* kImageMountPointTemplate;

  base::MessageLoopForIO base_loop_;
  brillo::BaseMessageLoop loop_{&base_loop_};
  brillo::AsynchronousSignalHandler async_signal_handler_;
  Subprocess subprocess_;
  FakeSystemState fake_system_state_;
};

class PostinstActionProcessorDelegate : public ActionProcessorDelegate {
 public:
  PostinstActionProcessorDelegate()
      : code_(ErrorCode::kError),
        code_set_(false) {}
  void ProcessingDone(const ActionProcessor* processor,
                      ErrorCode code) {
    MessageLoop::current()->BreakLoop();
  }
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ErrorCode code) {
    if (action->Type() == PostinstallRunnerAction::StaticType()) {
      code_ = code;
      code_set_ = true;
    }
  }
  ErrorCode code_;
  bool code_set_;
};

TEST_F(PostinstallRunnerActionTest, RunAsRootSimpleTest) {
  DoTest(true, 0, false);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootPowerwashRequiredTest) {
  DoTest(true, 0, true);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootCantMountTest) {
  DoTest(false, 0, true);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootErrScriptTest) {
  DoTest(true, 1, false);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootFirmwareBErrScriptTest) {
  DoTest(true, 3, false);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootFirmwareROErrScriptTest) {
  DoTest(true, 4, false);
}

const char* PostinstallRunnerActionTest::kImageMountPointTemplate =
    "au_destination-XXXXXX";

void PostinstallRunnerActionTest::DoTest(
    bool do_losetup,
    int err_code,
    bool powerwash_required) {
  ASSERT_EQ(0, getuid()) << "Run me as root. Ideally don't run other tests "
                         << "as root, tho.";
  // True if the post-install action is expected to succeed.
  bool should_succeed = do_losetup && !err_code;

  string orig_cwd;
  {
    vector<char> buf(1000);
    ASSERT_EQ(buf.data(), getcwd(buf.data(), buf.size()));
    orig_cwd = string(buf.data(), strlen(buf.data()));
  }

  // Create a unique named working directory and chdir into it.
  string cwd;
  ASSERT_TRUE(utils::MakeTempDirectory(
          "postinstall_runner_action_unittest-XXXXXX",
          &cwd));
  ASSERT_EQ(0, test_utils::Chdir(cwd));

  // Create a 10MiB sparse file to be used as image; format it as ext2.
  ASSERT_EQ(0, System(
          "dd if=/dev/zero of=image.dat seek=10485759 bs=1 count=1 "
          "status=none"));
  ASSERT_EQ(0, System("mkfs.ext2 -F image.dat"));

  // Create a uniquely named image mount point, mount the image.
  ASSERT_EQ(0, System(string("mkdir -p ") + kStatefulPartition));
  string mountpoint;
  ASSERT_TRUE(utils::MakeTempDirectory(
          string(kStatefulPartition) + "/" + kImageMountPointTemplate,
          &mountpoint));
  ASSERT_EQ(0, System(string("mount -o loop image.dat ") + mountpoint));

  // Generate a fake postinst script inside the image.
  string script = (err_code ?
                   base::StringPrintf("#!/bin/bash\nexit %d", err_code) :
                   base::StringPrintf(
                       "#!/bin/bash\n"
                       "mount | grep au_postint_mount | grep ext2\n"
                       "if [ $? -eq 0 ]; then\n"
                       "  touch %s/postinst_called\n"
                       "fi\n",
                       cwd.c_str()));
  const string script_file_name = mountpoint + "/postinst";
  ASSERT_TRUE(WriteFileString(script_file_name, script));
  ASSERT_EQ(0, System(string("chmod a+x ") + script_file_name));

  // Unmount image; do not remove the uniquely named directory as it will be
  // reused during the test.
  ASSERT_TRUE(utils::UnmountFilesystem(mountpoint));

  // get a loop device we can use for the install device
  string dev = "/dev/null";

  unique_ptr<test_utils::ScopedLoopbackDeviceBinder> loop_releaser;
  if (do_losetup) {
    loop_releaser.reset(new test_utils::ScopedLoopbackDeviceBinder(
            cwd + "/image.dat", &dev));
  }

  // We use a test-specific powerwash marker file, to avoid race conditions.
  string powerwash_marker_file = mountpoint + "/factory_install_reset";
  LOG(INFO) << ">>> powerwash_marker_file=" << powerwash_marker_file;

  ActionProcessor processor;
  ObjectFeederAction<InstallPlan> feeder_action;
  InstallPlan::Partition part;
  part.name = "part";
  part.target_path = dev;
  part.run_postinstall = true;
  InstallPlan install_plan;
  install_plan.partitions = {part};
  install_plan.download_url = "http://devserver:8080/update";
  install_plan.powerwash_required = powerwash_required;
  feeder_action.set_obj(install_plan);
  PostinstallRunnerAction runner_action(&fake_system_state_,
                                        powerwash_marker_file.c_str());
  BondActions(&feeder_action, &runner_action);
  ObjectCollectorAction<InstallPlan> collector_action;
  BondActions(&runner_action, &collector_action);
  PostinstActionProcessorDelegate delegate;
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&runner_action);
  processor.EnqueueAction(&collector_action);
  processor.set_delegate(&delegate);

  loop_.PostTask(FROM_HERE,
                 base::Bind([&processor] { processor.StartProcessing(); }));
  loop_.Run();
  ASSERT_FALSE(processor.IsRunning());

  EXPECT_TRUE(delegate.code_set_);
  EXPECT_EQ(should_succeed, delegate.code_ == ErrorCode::kSuccess);
  if (should_succeed)
    EXPECT_TRUE(install_plan == collector_action.object());

  const base::FilePath kPowerwashMarkerPath(powerwash_marker_file);
  string actual_cmd;
  if (should_succeed && powerwash_required) {
    EXPECT_TRUE(base::ReadFileToString(kPowerwashMarkerPath, &actual_cmd));
    EXPECT_EQ(kPowerwashCommand, actual_cmd);
  } else {
    EXPECT_FALSE(
        base::ReadFileToString(kPowerwashMarkerPath, &actual_cmd));
  }

  if (err_code == 2)
    EXPECT_EQ(ErrorCode::kPostinstallBootedFromFirmwareB, delegate.code_);

  struct stat stbuf;
  int rc = lstat((string(cwd) + "/postinst_called").c_str(), &stbuf);
  if (should_succeed)
    ASSERT_EQ(0, rc);
  else
    ASSERT_LT(rc, 0);

  if (do_losetup) {
    loop_releaser.reset(nullptr);
  }

  // Remove unique stateful directory.
  ASSERT_EQ(0, System(string("rm -fr ") + mountpoint));

  // Remove the temporary work directory.
  ASSERT_EQ(0, test_utils::Chdir(orig_cwd));
  ASSERT_EQ(0, System(string("rm -fr ") + cwd));
}

// Death tests don't seem to be working on Hardy
TEST_F(PostinstallRunnerActionTest, DISABLED_RunAsRootDeathTest) {
  ASSERT_EQ(0, getuid());
  PostinstallRunnerAction runner_action(&fake_system_state_);
  ASSERT_DEATH({ runner_action.TerminateProcessing(); },
               "postinstall_runner_action.h:.*] Check failed");
}

}  // namespace chromeos_update_engine
