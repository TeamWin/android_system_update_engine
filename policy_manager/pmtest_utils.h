// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PMTEST_UTILS_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PMTEST_UTILS_H_

#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>
#include <gtest/gtest.h>

#include "update_engine/policy_manager/variable.h"

// Convenience macros for checking null-ness of pointers.
//
// Purportedly, gtest should support pointer comparison when the first argument
// is a pointer (e.g. NULL). It is therefore appropriate to use
// {ASSERT,EXPECT}_{EQ,NE} for our purposes. However, gtest fails to realize
// that NULL is a pointer when used with the _NE variants, unless we explicitly
// cast it to a pointer type, and so we inject this casting.
//
// Note that checking nullness of the content of a scoped_ptr requires getting
// the inner pointer value via get().
#define PMTEST_ASSERT_NULL(p) ASSERT_EQ(NULL, p)
#define PMTEST_ASSERT_NOT_NULL(p) ASSERT_NE(reinterpret_cast<void*>(NULL), p)
#define PMTEST_EXPECT_NULL(p) EXPECT_EQ(NULL, p)
#define PMTEST_EXPECT_NOT_NULL(p) EXPECT_NE(reinterpret_cast<void*>(NULL), p)


namespace chromeos_policy_manager {

// A help class with common functionality for use in Policy Manager testing.
class PmTestUtils {
 public:
  // A default timeout to use when making various queries.
  static const base::TimeDelta DefaultTimeout() {
    return base::TimeDelta::FromSeconds(kDefaultTimeoutInSeconds);
  }

  // Calls GetValue on |variable| and expects its result to be |expected|.
  template<typename T>
  static void ExpectVariableHasValue(const T& expected, Variable<T>* variable) {
    PMTEST_ASSERT_NOT_NULL(variable);
    scoped_ptr<const T> value(variable->GetValue(DefaultTimeout(), nullptr));
    PMTEST_ASSERT_NOT_NULL(value.get()) << "Variable: " << variable->GetName();
    EXPECT_EQ(expected, *value) << "Variable: " << variable->GetName();
  }

  // Calls GetValue on |variable| and expects its result to be null.
  template<typename T>
  static void ExpectVariableNotSet(Variable<T>* variable) {
    PMTEST_ASSERT_NOT_NULL(variable);
    scoped_ptr<const T> value(variable->GetValue(DefaultTimeout(), nullptr));
    PMTEST_EXPECT_NULL(value.get()) << "Variable: " << variable->GetName();
  }

 private:
  static const unsigned kDefaultTimeoutInSeconds;
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_PMTEST_UTILS_H_
