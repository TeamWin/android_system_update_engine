//
// Copyright (C) 2009 The Android Open Source Project
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

#include "update_engine/common/action_processor.h"

#include <gtest/gtest.h>
#include <string>
#include "update_engine/common/action.h"

using std::string;

namespace chromeos_update_engine {

using chromeos_update_engine::ActionPipe;

class ActionProcessorTestAction;

template<>
class ActionTraits<ActionProcessorTestAction> {
 public:
  typedef string OutputObjectType;
  typedef string InputObjectType;
};

// This is a simple Action class for testing.
class ActionProcessorTestAction : public Action<ActionProcessorTestAction> {
 public:
  typedef string InputObjectType;
  typedef string OutputObjectType;
  ActionPipe<string>* in_pipe() { return in_pipe_.get(); }
  ActionPipe<string>* out_pipe() { return out_pipe_.get(); }
  ActionProcessor* processor() { return processor_; }
  void PerformAction() {}
  void CompleteAction() {
    ASSERT_TRUE(processor());
    processor()->ActionComplete(this, ErrorCode::kSuccess);
  }
  string Type() const { return "ActionProcessorTestAction"; }
};

class ActionProcessorTest : public ::testing::Test { };

// This test creates two simple Actions and sends a message via an ActionPipe
// from one to the other.
TEST(ActionProcessorTest, SimpleTest) {
  ActionProcessorTestAction action;
  ActionProcessor action_processor;
  EXPECT_FALSE(action_processor.IsRunning());
  action_processor.EnqueueAction(&action);
  EXPECT_FALSE(action_processor.IsRunning());
  EXPECT_FALSE(action.IsRunning());
  action_processor.StartProcessing();
  EXPECT_TRUE(action_processor.IsRunning());
  EXPECT_TRUE(action.IsRunning());
  EXPECT_EQ(action_processor.current_action(), &action);
  action.CompleteAction();
  EXPECT_FALSE(action_processor.IsRunning());
  EXPECT_FALSE(action.IsRunning());
}

namespace {
class MyActionProcessorDelegate : public ActionProcessorDelegate {
 public:
  explicit MyActionProcessorDelegate(const ActionProcessor* processor)
      : processor_(processor),
        processing_done_called_(false),
        processing_stopped_called_(false),
        action_completed_called_(false),
        action_exit_code_(ErrorCode::kError) {}

  virtual void ProcessingDone(const ActionProcessor* processor,
                              ErrorCode code) {
    EXPECT_EQ(processor_, processor);
    EXPECT_FALSE(processing_done_called_);
    processing_done_called_ = true;
  }
  virtual void ProcessingStopped(const ActionProcessor* processor) {
    EXPECT_EQ(processor_, processor);
    EXPECT_FALSE(processing_stopped_called_);
    processing_stopped_called_ = true;
  }
  virtual void ActionCompleted(ActionProcessor* processor,
                               AbstractAction* action,
                               ErrorCode code) {
    EXPECT_EQ(processor_, processor);
    EXPECT_FALSE(action_completed_called_);
    action_completed_called_ = true;
    action_exit_code_ = code;
  }

  const ActionProcessor* processor_;
  bool processing_done_called_;
  bool processing_stopped_called_;
  bool action_completed_called_;
  ErrorCode action_exit_code_;
};
}  // namespace

TEST(ActionProcessorTest, DelegateTest) {
  ActionProcessorTestAction action;
  ActionProcessor action_processor;
  MyActionProcessorDelegate delegate(&action_processor);
  action_processor.set_delegate(&delegate);

  action_processor.EnqueueAction(&action);
  action_processor.StartProcessing();
  action.CompleteAction();
  action_processor.set_delegate(nullptr);
  EXPECT_TRUE(delegate.processing_done_called_);
  EXPECT_TRUE(delegate.action_completed_called_);
}

TEST(ActionProcessorTest, StopProcessingTest) {
  ActionProcessorTestAction action;
  ActionProcessor action_processor;
  MyActionProcessorDelegate delegate(&action_processor);
  action_processor.set_delegate(&delegate);

  action_processor.EnqueueAction(&action);
  action_processor.StartProcessing();
  action_processor.StopProcessing();
  action_processor.set_delegate(nullptr);
  EXPECT_TRUE(delegate.processing_stopped_called_);
  EXPECT_FALSE(delegate.action_completed_called_);
  EXPECT_FALSE(action_processor.IsRunning());
  EXPECT_EQ(nullptr, action_processor.current_action());
}

TEST(ActionProcessorTest, ChainActionsTest) {
  ActionProcessorTestAction action1, action2;
  ActionProcessor action_processor;
  action_processor.EnqueueAction(&action1);
  action_processor.EnqueueAction(&action2);
  action_processor.StartProcessing();
  EXPECT_EQ(&action1, action_processor.current_action());
  EXPECT_TRUE(action_processor.IsRunning());
  action1.CompleteAction();
  EXPECT_EQ(&action2, action_processor.current_action());
  EXPECT_TRUE(action_processor.IsRunning());
  action2.CompleteAction();
  EXPECT_EQ(nullptr, action_processor.current_action());
  EXPECT_FALSE(action_processor.IsRunning());
}

TEST(ActionProcessorTest, DtorTest) {
  ActionProcessorTestAction action1, action2;
  {
    ActionProcessor action_processor;
    action_processor.EnqueueAction(&action1);
    action_processor.EnqueueAction(&action2);
    action_processor.StartProcessing();
  }
  EXPECT_EQ(nullptr, action1.processor());
  EXPECT_FALSE(action1.IsRunning());
  EXPECT_EQ(nullptr, action2.processor());
  EXPECT_FALSE(action2.IsRunning());
}

TEST(ActionProcessorTest, DefaultDelegateTest) {
  // Just make sure it doesn't crash
  ActionProcessorTestAction action;
  ActionProcessor action_processor;
  ActionProcessorDelegate delegate;
  action_processor.set_delegate(&delegate);

  action_processor.EnqueueAction(&action);
  action_processor.StartProcessing();
  action.CompleteAction();

  action_processor.EnqueueAction(&action);
  action_processor.StartProcessing();
  action_processor.StopProcessing();

  action_processor.set_delegate(nullptr);
}

}  // namespace chromeos_update_engine
