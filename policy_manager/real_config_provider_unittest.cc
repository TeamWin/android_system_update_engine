// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/real_config_provider.h"

#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "update_engine/constants.h"
#include "update_engine/fake_hardware.h"
#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/test_utils.h"

using base::TimeDelta;
using chromeos_update_engine::WriteFileString;
using std::string;

namespace chromeos_policy_manager {

class PmRealConfigProviderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(root_dir_.CreateUniqueTempDir());
    provider_.reset(new RealConfigProvider(&fake_hardware_));
    provider_->SetRootPrefix(root_dir_.path().value());
  }

  void WriteStatefulConfig(const string& config) {
    base::FilePath kFile(root_dir_.path().value()
                         + chromeos_update_engine::kStatefulPartition
                         + "/etc/policy_manager.conf");
    ASSERT_TRUE(base::CreateDirectory(kFile.DirName()));
    ASSERT_TRUE(WriteFileString(kFile.value(), config));
  }

  void WriteRootfsConfig(const string& config) {
    base::FilePath kFile(root_dir_.path().value()
                         + "/etc/policy_manager.conf");
    ASSERT_TRUE(base::CreateDirectory(kFile.DirName()));
    ASSERT_TRUE(WriteFileString(kFile.value(), config));
  }

  scoped_ptr<RealConfigProvider> provider_;
  chromeos_update_engine::FakeHardware fake_hardware_;
  TimeDelta default_timeout_ = TimeDelta::FromSeconds(1);
  base::ScopedTempDir root_dir_;
};

TEST_F(PmRealConfigProviderTest, InitTest) {
  EXPECT_TRUE(provider_->Init());
  PMTEST_EXPECT_NOT_NULL(provider_->var_is_oobe_enabled());
}

TEST_F(PmRealConfigProviderTest, NoFileFoundReturnsDefault) {
  EXPECT_TRUE(provider_->Init());
  PmTestUtils::ExpectVariableHasValue(true, provider_->var_is_oobe_enabled());
}

TEST_F(PmRealConfigProviderTest, DontReadStatefulInNormalMode) {
  fake_hardware_.SetIsNormalBootMode(true);
  WriteStatefulConfig("is_oobe_enabled=false");

  EXPECT_TRUE(provider_->Init());
  PmTestUtils::ExpectVariableHasValue(true, provider_->var_is_oobe_enabled());
}

TEST_F(PmRealConfigProviderTest, ReadStatefulInDevMode) {
  fake_hardware_.SetIsNormalBootMode(false);
  WriteRootfsConfig("is_oobe_enabled=true");
  // Since the stateful is present, this should read that one.
  WriteStatefulConfig("is_oobe_enabled=false");

  EXPECT_TRUE(provider_->Init());
  PmTestUtils::ExpectVariableHasValue(false, provider_->var_is_oobe_enabled());
}

TEST_F(PmRealConfigProviderTest, ReadRootfsIfStatefulNotFound) {
  fake_hardware_.SetIsNormalBootMode(false);
  WriteRootfsConfig("is_oobe_enabled=false");

  EXPECT_TRUE(provider_->Init());
  PmTestUtils::ExpectVariableHasValue(false, provider_->var_is_oobe_enabled());
}

}  // namespace chromeos_policy_manager
