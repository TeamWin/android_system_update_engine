// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H

#include <string>

#include <base/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace chromeos_policy_manager {

// Interface to a Policy Manager variable to be implemented by the providers.
template<typename T>
class Variable {
 public:
  virtual ~Variable() {}

 protected:
  friend class PMRandomProviderTest;
  FRIEND_TEST(PMRandomProviderTest, GetRandomValues);

  // Gets the current value of the variable. The current value is copied to a
  // new object and returned. The caller of this method owns the object and
  // should delete it.
  //
  // In case of and error getting the current value or the |timeout| timeout is
  // exceeded, a NULL value is returned and the |errmsg| is set.
  //
  // The caller can pass a NULL value for |errmsg|, in which case the error
  // message won't be set.
  virtual const T* GetValue(base::TimeDelta timeout, std::string* errmsg) = 0;
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_VARIABLE_H
