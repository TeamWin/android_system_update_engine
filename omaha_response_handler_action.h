// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_OMAHA_RESPONSE_HANDLER_ACTION_H_
#define UPDATE_ENGINE_OMAHA_RESPONSE_HANDLER_ACTION_H_

#include <string>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/action.h"
#include "update_engine/install_plan.h"
#include "update_engine/omaha_request_action.h"
#include "update_engine/system_state.h"

// This class reads in an Omaha response and converts what it sees into
// an install plan which is passed out.

namespace chromeos_update_engine {

class OmahaResponseHandlerAction;

template<>
class ActionTraits<OmahaResponseHandlerAction> {
 public:
  typedef OmahaResponse InputObjectType;
  typedef InstallPlan OutputObjectType;
};

class OmahaResponseHandlerAction : public Action<OmahaResponseHandlerAction> {
 public:
  static const char kDeadlineFile[];

  explicit OmahaResponseHandlerAction(SystemState* system_state);

  typedef ActionTraits<OmahaResponseHandlerAction>::InputObjectType
      InputObjectType;
  typedef ActionTraits<OmahaResponseHandlerAction>::OutputObjectType
      OutputObjectType;
  void PerformAction() override;

  // This is a synchronous action, and thus TerminateProcessing() should
  // never be called
  void TerminateProcessing() override { CHECK(false); }

  // For unit-testing
  void set_boot_device(const std::string& boot_device) {
    boot_device_ = boot_device;
  }

  bool GotNoUpdateResponse() const { return got_no_update_response_; }
  const InstallPlan& install_plan() const { return install_plan_; }

  // Debugging/logging
  static std::string StaticType() { return "OmahaResponseHandlerAction"; }
  std::string Type() const override { return StaticType(); }
  void set_key_path(const std::string& path) { key_path_ = path; }

 private:
  // Returns true if payload hash checks are mandatory based on the state
  // of the system and the contents of the Omaha response. False otherwise.
  bool AreHashChecksMandatory(const OmahaResponse& response);

  // Global system context.
  SystemState* system_state_;

  // set to non-empty in unit tests
  std::string boot_device_;

  // The install plan, if we have an update.
  InstallPlan install_plan_;

  // True only if we got a response and the response said no updates
  bool got_no_update_response_;

  // Public key path to use for payload verification.
  std::string key_path_;

  // File used for communication deadline to Chrome.
  const std::string deadline_file_;

  // Special ctor + friend declarations for testing purposes.
  OmahaResponseHandlerAction(SystemState* system_state,
                             const std::string& deadline_file);

  friend class OmahaResponseHandlerActionTest;

  FRIEND_TEST(UpdateAttempterTest, CreatePendingErrorEventResumedTest);

  DISALLOW_COPY_AND_ASSIGN(OmahaResponseHandlerAction);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_OMAHA_RESPONSE_HANDLER_ACTION_H_
