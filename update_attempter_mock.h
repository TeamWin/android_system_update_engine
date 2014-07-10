// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_ATTEMPTER_MOCK_H_
#define UPDATE_ENGINE_UPDATE_ATTEMPTER_MOCK_H_

#include <string>

#include "update_engine/update_attempter.h"

#include <gmock/gmock.h>

namespace chromeos_update_engine {

class UpdateAttempterMock : public UpdateAttempter {
 public:
  using UpdateAttempter::UpdateAttempter;

  MOCK_METHOD4(Update, void(const std::string& app_version,
                            const std::string& omaha_url,
                            bool obey_proxies,
                            bool interactive));

  MOCK_METHOD5(GetStatus, bool(int64_t* last_checked_time,
                               double* progress,
                               std::string* current_operation,
                               std::string* new_version,
                               int64_t* new_size));

  MOCK_METHOD1(GetBootTimeAtUpdate, bool(base::Time* out_boot_time));

  MOCK_CONST_METHOD0(consecutive_failed_update_checks, unsigned int(void));

  MOCK_CONST_METHOD0(server_dictated_poll_interval, unsigned int(void));
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_UPDATE_ATTEMPTER_MOCK_H_
