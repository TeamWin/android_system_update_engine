// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/real_config_provider.h"

#include <base/logging.h>

#include "update_engine/constants.h"
#include "update_engine/simple_key_value_store.h"
#include "update_engine/update_manager/generic_variables.h"
#include "update_engine/utils.h"

using chromeos_update_engine::KeyValueStore;
using std::string;

namespace {

const char* kConfigFilePath = "/etc/update_manager.conf";

// Config options:
const char* kConfigOptsIsOOBEEnabled = "is_oobe_enabled";

}  // namespace

namespace chromeos_update_manager {

bool RealConfigProvider::Init() {
  KeyValueStore store;

  if (hardware_->IsNormalBootMode()) {
    store.Load(root_prefix_ + kConfigFilePath);
  } else {
    if (store.Load(root_prefix_ + chromeos_update_engine::kStatefulPartition +
                   kConfigFilePath)) {
      LOG(INFO) << "UpdateManager Config loaded from stateful partition.";
    } else {
      store.Load(root_prefix_ + kConfigFilePath);
    }
  }

  bool is_oobe_enabled;
  if (!store.GetBoolean(kConfigOptsIsOOBEEnabled, &is_oobe_enabled))
    is_oobe_enabled = true;  // Default value.
  var_is_oobe_enabled_.reset(
      new ConstCopyVariable<bool>(kConfigOptsIsOOBEEnabled, is_oobe_enabled));

  return true;
}

}  // namespace chromeos_update_manager
