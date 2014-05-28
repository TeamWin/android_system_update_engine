// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_MOCK_VARIABLE_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_MOCK_VARIABLE_H_

#include <gmock/gmock.h>

#include "update_engine/update_manager/variable.h"

namespace chromeos_update_manager {

// This is a generic mock of the Variable class.
template<typename T>
class MockVariable : public Variable<T> {
 public:
  using Variable<T>::Variable;

  MOCK_METHOD2_T(GetValue, const T*(base::TimeDelta, std::string*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVariable);
};

}  // namespace chromeos_update_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_MANAGER_MOCK_VARIABLE_H_
