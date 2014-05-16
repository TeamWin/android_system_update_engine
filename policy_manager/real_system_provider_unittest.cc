// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_system_provider.h"

#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>
#include <gtest/gtest.h>

#include "update_engine/fake_hardware.h"
#include "update_engine/policy_manager/pmtest_utils.h"

namespace chromeos_policy_manager {

class PmRealSystemProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    provider_.reset(new RealSystemProvider(&fake_hardware_));
    EXPECT_TRUE(provider_->Init());
  }

  chromeos_update_engine::FakeHardware fake_hardware_;
  scoped_ptr<RealSystemProvider> provider_;
};

TEST_F(PmRealSystemProviderTest, InitTest) {
  PMTEST_EXPECT_NOT_NULL(provider_->var_is_normal_boot_mode());
  PMTEST_EXPECT_NOT_NULL(provider_->var_is_official_build());
  PMTEST_EXPECT_NOT_NULL(provider_->var_is_oobe_complete());
}

TEST_F(PmRealSystemProviderTest, IsOOBECompleteTrue) {
  fake_hardware_.SetIsOOBEComplete(base::Time());
  PmTestUtils::ExpectVariableHasValue(true, provider_->var_is_oobe_complete());
}

TEST_F(PmRealSystemProviderTest, IsOOBECompleteFalse) {
  fake_hardware_.UnsetIsOOBEComplete();
  PmTestUtils::ExpectVariableHasValue(false, provider_->var_is_oobe_complete());
}

}  // namespace chromeos_policy_manager
