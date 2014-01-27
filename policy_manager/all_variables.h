// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_ALL_VARIABLES_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_ALL_VARIABLES_H

// List of globally available variables exposed by the different providers.
//
// Each state provider must implement a header file with the suffix "_vars.h",
// which declares all the variables owned by this provider declared as extern
// global pointers.
//
//   extern Variable<SomeType>* var_providername_variablename;
//
// This file is just an aggregate of all variable declarations
// from the different providers.

#include "policy_manager/random_vars.h"

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_ALL_VARIABLES_H
