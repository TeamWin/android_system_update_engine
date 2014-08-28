// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/real_system_provider.h"

#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>
#include <gtest/gtest.h>

#include "update_engine/fake_hardware.h"
#include "update_engine/update_manager/umtest_utils.h"

namespace chromeos_update_manager {

class UmRealSystemProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    provider_.reset(new RealSystemProvider(&fake_hardware_));
    EXPECT_TRUE(provider_->Init());
  }

  chromeos_update_engine::FakeHardware fake_hardware_;
  scoped_ptr<RealSystemProvider> provider_;
};

TEST_F(UmRealSystemProviderTest, InitTest) {
  EXPECT_NE(nullptr, provider_->var_is_normal_boot_mode());
  EXPECT_NE(nullptr, provider_->var_is_official_build());
  EXPECT_NE(nullptr, provider_->var_is_oobe_complete());
}

TEST_F(UmRealSystemProviderTest, IsOOBECompleteTrue) {
  fake_hardware_.SetIsOOBEComplete(base::Time());
  UmTestUtils::ExpectVariableHasValue(true, provider_->var_is_oobe_complete());
}

TEST_F(UmRealSystemProviderTest, IsOOBECompleteFalse) {
  fake_hardware_.UnsetIsOOBEComplete();
  UmTestUtils::ExpectVariableHasValue(false, provider_->var_is_oobe_complete());
}

}  // namespace chromeos_update_manager
