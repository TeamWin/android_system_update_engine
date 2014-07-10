// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_ACTION_MOCK_H_
#define UPDATE_ENGINE_ACTION_MOCK_H_

#include <string>

#include <gmock/gmock.h>

#include "update_engine/action.h"

namespace chromeos_update_engine {

class ActionMock;

template<>
class ActionTraits<ActionMock> {
 public:
  typedef NoneType OutputObjectType;
  typedef NoneType InputObjectType;
};

class ActionMock : public Action<ActionMock> {
 public:
  MOCK_METHOD0(PerformAction, void());
  MOCK_CONST_METHOD0(Type, std::string());
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_ACTION_MOCK_H_
