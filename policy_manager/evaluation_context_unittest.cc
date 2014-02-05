// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>
#include <string>

#include "update_engine/policy_manager/evaluation_context.h"
#include "update_engine/policy_manager/generic_variables.h"
#include "update_engine/policy_manager/pmtest_utils.h"
#include "update_engine/policy_manager/fake_variable.h"

using std::string;

namespace chromeos_policy_manager {

class PmEvaluationContextTest : public ::testing::Test {
 public:
  PmEvaluationContextTest() : fake_int_var_("fake_int") {}

 protected:
  virtual void SetUp() {
    eval_ctx_.reset(new EvaluationContext());
  }

  scoped_ptr<EvaluationContext> eval_ctx_;
  FakeVariable<int> fake_int_var_;
};

TEST_F(PmEvaluationContextTest, GetValueFails) {
  // FakeVariable is initialized as returning NULL.
  PMTEST_EXPECT_NULL(eval_ctx_->GetValue(&fake_int_var_));
}

TEST_F(PmEvaluationContextTest, GetValueFailsWithInvalidVar) {
  PMTEST_EXPECT_NULL(eval_ctx_->GetValue(
      reinterpret_cast<Variable<int>*>(NULL)));
}

TEST_F(PmEvaluationContextTest, GetValueReturns) {
  const int* p_fake_int;

  fake_int_var_.reset(new int(42));
  p_fake_int = eval_ctx_->GetValue(&fake_int_var_);
  PMTEST_ASSERT_NOT_NULL(p_fake_int);
  EXPECT_EQ(42, *p_fake_int);
}

TEST_F(PmEvaluationContextTest, GetValueCached) {
  const int* p_fake_int;

  fake_int_var_.reset(new int(42));
  p_fake_int = eval_ctx_->GetValue(&fake_int_var_);

  // Check that if the variable changes, the EvaluationContext keeps returning
  // the cached value.
  fake_int_var_.reset(new int(5));

  p_fake_int = eval_ctx_->GetValue(&fake_int_var_);
  PMTEST_ASSERT_NOT_NULL(p_fake_int);
  EXPECT_EQ(42, *p_fake_int);
}

TEST_F(PmEvaluationContextTest, GetValueDontCacheNULL) {
  const int* p_fake_int = eval_ctx_->GetValue(&fake_int_var_);
  PMTEST_EXPECT_NULL(p_fake_int);

  fake_int_var_.reset(new int(42));
  // A second attempt to read the variable should work even on the same
  // EvaluationContext.
  p_fake_int = eval_ctx_->GetValue(&fake_int_var_);
  PMTEST_ASSERT_NOT_NULL(p_fake_int);
  EXPECT_EQ(42, *p_fake_int);
}

TEST_F(PmEvaluationContextTest, GetValueMixedTypes) {
  FakeVariable<string> fake_string_var_("fake_string");
  const int* p_fake_int;
  const string* p_fake_string;

  fake_int_var_.reset(new int(42));
  fake_string_var_.reset(new string("Hello world!"));
  // Check that the EvaluationContext can handle multiple Variable types. This
  // is mostly a compile-time check due to the template nature of this method.
  p_fake_int = eval_ctx_->GetValue(&fake_int_var_);
  p_fake_string = eval_ctx_->GetValue(&fake_string_var_);

  PMTEST_ASSERT_NOT_NULL(p_fake_int);
  EXPECT_EQ(42, *p_fake_int);

  PMTEST_ASSERT_NOT_NULL(p_fake_string);
  EXPECT_EQ("Hello world!", *p_fake_string);
}

}  // namespace chromeos_policy_manager
