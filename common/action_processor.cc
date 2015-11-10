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

#include <string>

#include <base/logging.h>

#include "update_engine/common/action.h"

using std::string;

namespace chromeos_update_engine {

ActionProcessor::ActionProcessor()
    : current_action_(nullptr), delegate_(nullptr) {}

ActionProcessor::~ActionProcessor() {
  if (IsRunning()) {
    StopProcessing();
  }
  for (std::deque<AbstractAction*>::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    (*it)->SetProcessor(nullptr);
  }
}

void ActionProcessor::EnqueueAction(AbstractAction* action) {
  actions_.push_back(action);
  action->SetProcessor(this);
}

void ActionProcessor::StartProcessing() {
  CHECK(!IsRunning());
  if (!actions_.empty()) {
    current_action_ = actions_.front();
    LOG(INFO) << "ActionProcessor::StartProcessing: "
              << current_action_->Type();
    actions_.pop_front();
    current_action_->PerformAction();
  }
}

void ActionProcessor::StopProcessing() {
  CHECK(IsRunning());
  CHECK(current_action_);
  current_action_->TerminateProcessing();
  CHECK(current_action_);
  current_action_->SetProcessor(nullptr);
  LOG(INFO) << "ActionProcessor::StopProcessing: aborted "
            << current_action_->Type();
  current_action_ = nullptr;
  if (delegate_)
    delegate_->ProcessingStopped(this);
}

void ActionProcessor::ActionComplete(AbstractAction* actionptr,
                                     ErrorCode code) {
  CHECK_EQ(actionptr, current_action_);
  if (delegate_)
    delegate_->ActionCompleted(this, actionptr, code);
  string old_type = current_action_->Type();
  current_action_->ActionCompleted(code);
  current_action_->SetProcessor(nullptr);
  current_action_ = nullptr;
  if (actions_.empty()) {
    LOG(INFO) << "ActionProcessor::ActionComplete: finished last action of"
                 " type " << old_type;
  } else if (code != ErrorCode::kSuccess) {
    LOG(INFO) << "ActionProcessor::ActionComplete: " << old_type
              << " action failed. Aborting processing.";
    actions_.clear();
  }
  if (actions_.empty()) {
    LOG(INFO) << "ActionProcessor::ActionComplete: finished last action of"
                 " type " << old_type;
    if (delegate_) {
      delegate_->ProcessingDone(this, code);
    }
    return;
  }
  current_action_ = actions_.front();
  actions_.pop_front();
  LOG(INFO) << "ActionProcessor::ActionComplete: finished " << old_type
            << ", starting " << current_action_->Type();
  current_action_->PerformAction();
}

}  // namespace chromeos_update_engine
