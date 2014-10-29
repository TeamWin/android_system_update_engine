// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_MOCK_POLICY_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_MOCK_POLICY_H_

#include <string>

#include <gmock/gmock.h>

#include "update_engine/update_manager/default_policy.h"
#include "update_engine/update_manager/policy.h"

namespace chromeos_update_manager {

// A mocked implementation of Policy.
class MockPolicy : public Policy {
 public:
  explicit MockPolicy(chromeos_update_engine::ClockInterface* clock)
      : default_policy_(clock) {
    // We defer to the corresponding DefaultPolicy methods, by default.
    ON_CALL(*this, UpdateCheckAllowed(testing::_, testing::_, testing::_,
                                      testing::_))
        .WillByDefault(testing::Invoke(
                &default_policy_, &DefaultPolicy::UpdateCheckAllowed));
    ON_CALL(*this, UpdateCanStart(testing::_, testing::_, testing::_,
                                  testing::_, testing::_))
        .WillByDefault(testing::Invoke(
                &default_policy_, &DefaultPolicy::UpdateCanStart));
    ON_CALL(*this, UpdateDownloadAllowed(testing::_, testing::_, testing::_,
                                         testing::_))
        .WillByDefault(testing::Invoke(
                &default_policy_, &DefaultPolicy::UpdateDownloadAllowed));
    ON_CALL(*this, P2PEnabled(testing::_, testing::_, testing::_, testing::_))
        .WillByDefault(testing::Invoke(
                &default_policy_, &DefaultPolicy::P2PEnabled));
    ON_CALL(*this, P2PEnabledChanged(testing::_, testing::_, testing::_,
                                     testing::_, testing::_))
        .WillByDefault(testing::Invoke(
                &default_policy_, &DefaultPolicy::P2PEnabledChanged));
  }

  MockPolicy() : MockPolicy(nullptr) {}
  virtual ~MockPolicy() {}

  // Policy overrides.
  MOCK_CONST_METHOD4(UpdateCheckAllowed,
                     EvalStatus(EvaluationContext*, State*, std::string*,
                                UpdateCheckParams*));

  MOCK_CONST_METHOD5(UpdateCanStart,
                     EvalStatus(EvaluationContext*, State*, std::string*,
                                UpdateDownloadParams*, UpdateState));

  MOCK_CONST_METHOD4(UpdateDownloadAllowed,
                     EvalStatus(EvaluationContext*, State*, std::string*,
                                bool*));

  MOCK_CONST_METHOD4(P2PEnabled,
                     EvalStatus(EvaluationContext*, State*, std::string*,
                                bool*));

  MOCK_CONST_METHOD5(P2PEnabledChanged,
                     EvalStatus(EvaluationContext*, State*, std::string*,
                                bool*, bool));

 protected:
  // Policy override.
  std::string PolicyName() const override { return "MockPolicy"; }

 private:
  DefaultPolicy default_policy_;

  DISALLOW_COPY_AND_ASSIGN(MockPolicy);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_MOCK_POLICY_H_
