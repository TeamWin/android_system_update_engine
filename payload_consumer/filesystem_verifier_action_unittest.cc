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

#include "update_engine/payload_consumer/filesystem_verifier_action.h"

#include <fcntl.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/bind_lambda.h>
#include <brillo/message_loops/fake_message_loop.h>
#include <brillo/message_loops/message_loop_utils.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/payload_constants.h"

using brillo::MessageLoop;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class FilesystemVerifierActionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_.SetAsCurrent();
  }

  void TearDown() override {
    EXPECT_EQ(0, brillo::MessageLoopRunMaxIterations(&loop_, 1));
  }

  // Returns true iff test has completed successfully.
  bool DoTest(bool terminate_early, bool hash_fail);

  brillo::FakeMessageLoop loop_{nullptr};
};

class FilesystemVerifierActionTestDelegate : public ActionProcessorDelegate {
 public:
  FilesystemVerifierActionTestDelegate()
      : ran_(false), code_(ErrorCode::kError) {}

  void ProcessingDone(const ActionProcessor* processor, ErrorCode code) {
    MessageLoop::current()->BreakLoop();
  }
  void ProcessingStopped(const ActionProcessor* processor) {
    MessageLoop::current()->BreakLoop();
  }
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ErrorCode code) {
    if (action->Type() == FilesystemVerifierAction::StaticType()) {
      ran_ = true;
      code_ = code;
      EXPECT_FALSE(static_cast<FilesystemVerifierAction*>(action)->src_stream_);
    } else if (action->Type() ==
               ObjectCollectorAction<InstallPlan>::StaticType()) {
      auto collector_action =
          static_cast<ObjectCollectorAction<InstallPlan>*>(action);
      install_plan_.reset(new InstallPlan(collector_action->object()));
    }
  }
  bool ran() const { return ran_; }
  ErrorCode code() const { return code_; }

  std::unique_ptr<InstallPlan> install_plan_;

 private:
  bool ran_;
  ErrorCode code_;
};

bool FilesystemVerifierActionTest::DoTest(bool terminate_early,
                                          bool hash_fail) {
  string a_loop_file;

  if (!(utils::MakeTempFile("a_loop_file.XXXXXX", &a_loop_file, nullptr))) {
    ADD_FAILURE();
    return false;
  }
  ScopedPathUnlinker a_loop_file_unlinker(a_loop_file);

  // Make random data for a.
  const size_t kLoopFileSize = 10 * 1024 * 1024 + 512;
  brillo::Blob a_loop_data(kLoopFileSize);
  test_utils::FillWithData(&a_loop_data);

  // Write data to disk
  if (!(test_utils::WriteFileVector(a_loop_file, a_loop_data))) {
    ADD_FAILURE();
    return false;
  }

  // Attach loop devices to the files
  string a_dev;
  test_utils::ScopedLoopbackDeviceBinder a_dev_releaser(
      a_loop_file, false, &a_dev);
  if (!(a_dev_releaser.is_bound())) {
    ADD_FAILURE();
    return false;
  }

  LOG(INFO) << "verifying: "  << a_loop_file << " (" << a_dev << ")";

  bool success = true;

  // Set up the action objects
  InstallPlan install_plan;
  install_plan.source_slot = 0;
  install_plan.target_slot = 1;
  InstallPlan::Partition part;
  part.name = "part";
  part.target_size = kLoopFileSize - (hash_fail ? 1 : 0);
  part.target_path = a_dev;
  if (!HashCalculator::RawHashOfData(a_loop_data, &part.target_hash)) {
    ADD_FAILURE();
    success = false;
  }
  part.source_size = kLoopFileSize;
  part.source_path = a_dev;
  if (!HashCalculator::RawHashOfData(a_loop_data, &part.source_hash)) {
    ADD_FAILURE();
    success = false;
  }
  install_plan.partitions = {part};

  auto feeder_action = std::make_unique<ObjectFeederAction<InstallPlan>>();
  feeder_action->set_obj(install_plan);
  auto copier_action = std::make_unique<FilesystemVerifierAction>();
  auto collector_action =
      std::make_unique<ObjectCollectorAction<InstallPlan>>();

  BondActions(feeder_action.get(), copier_action.get());
  BondActions(copier_action.get(), collector_action.get());

  ActionProcessor processor;
  FilesystemVerifierActionTestDelegate delegate;
  processor.set_delegate(&delegate);
  processor.EnqueueAction(std::move(feeder_action));
  processor.EnqueueAction(std::move(copier_action));
  processor.EnqueueAction(std::move(collector_action));

  loop_.PostTask(FROM_HERE,
                 base::Bind(
                     [](ActionProcessor* processor, bool terminate_early) {
                       processor->StartProcessing();
                       if (terminate_early) {
                         processor->StopProcessing();
                       }
                     },
                     base::Unretained(&processor),
                     terminate_early));
  loop_.Run();

  if (!terminate_early) {
    bool is_delegate_ran = delegate.ran();
    EXPECT_TRUE(is_delegate_ran);
    success = success && is_delegate_ran;
  } else {
    EXPECT_EQ(ErrorCode::kError, delegate.code());
    return (ErrorCode::kError == delegate.code());
  }
  if (hash_fail) {
    ErrorCode expected_exit_code = ErrorCode::kNewRootfsVerificationError;
    EXPECT_EQ(expected_exit_code, delegate.code());
    return (expected_exit_code == delegate.code());
  }
  EXPECT_EQ(ErrorCode::kSuccess, delegate.code());

  // Make sure everything in the out_image is there
  brillo::Blob a_out;
  if (!utils::ReadFile(a_dev, &a_out)) {
    ADD_FAILURE();
    return false;
  }
  const bool is_a_file_reading_eq =
      test_utils::ExpectVectorsEq(a_loop_data, a_out);
  EXPECT_TRUE(is_a_file_reading_eq);
  success = success && is_a_file_reading_eq;

  bool is_install_plan_eq = (*delegate.install_plan_ == install_plan);
  EXPECT_TRUE(is_install_plan_eq);
  success = success && is_install_plan_eq;
  return success;
}

class FilesystemVerifierActionTest2Delegate : public ActionProcessorDelegate {
 public:
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ErrorCode code) {
    if (action->Type() == FilesystemVerifierAction::StaticType()) {
      ran_ = true;
      code_ = code;
    }
  }
  bool ran_;
  ErrorCode code_;
};

TEST_F(FilesystemVerifierActionTest, MissingInputObjectTest) {
  ActionProcessor processor;
  FilesystemVerifierActionTest2Delegate delegate;

  processor.set_delegate(&delegate);

  auto copier_action = std::make_unique<FilesystemVerifierAction>();
  auto collector_action =
      std::make_unique<ObjectCollectorAction<InstallPlan>>();

  BondActions(copier_action.get(), collector_action.get());

  processor.EnqueueAction(std::move(copier_action));
  processor.EnqueueAction(std::move(collector_action));
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran_);
  EXPECT_EQ(ErrorCode::kError, delegate.code_);
}

TEST_F(FilesystemVerifierActionTest, NonExistentDriveTest) {
  ActionProcessor processor;
  FilesystemVerifierActionTest2Delegate delegate;

  processor.set_delegate(&delegate);

  InstallPlan install_plan;
  InstallPlan::Partition part;
  part.name = "nope";
  part.source_path = "/no/such/file";
  part.target_path = "/no/such/file";
  install_plan.partitions = {part};

  auto feeder_action = std::make_unique<ObjectFeederAction<InstallPlan>>();
  auto verifier_action = std::make_unique<FilesystemVerifierAction>();
  auto collector_action =
      std::make_unique<ObjectCollectorAction<InstallPlan>>();

  feeder_action->set_obj(install_plan);

  BondActions(verifier_action.get(), collector_action.get());

  processor.EnqueueAction(std::move(feeder_action));
  processor.EnqueueAction(std::move(verifier_action));
  processor.EnqueueAction(std::move(collector_action));
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran_);
  EXPECT_EQ(ErrorCode::kError, delegate.code_);
}

TEST_F(FilesystemVerifierActionTest, RunAsRootVerifyHashTest) {
  ASSERT_EQ(0U, getuid());
  EXPECT_TRUE(DoTest(false, false));
}

TEST_F(FilesystemVerifierActionTest, RunAsRootVerifyHashFailTest) {
  ASSERT_EQ(0U, getuid());
  EXPECT_TRUE(DoTest(false, true));
}

TEST_F(FilesystemVerifierActionTest, RunAsRootTerminateEarlyTest) {
  ASSERT_EQ(0U, getuid());
  EXPECT_TRUE(DoTest(true, false));
  // TerminateEarlyTest may leak some null callbacks from the Stream class.
  while (loop_.RunOnce(false)) {}
}

}  // namespace chromeos_update_engine
