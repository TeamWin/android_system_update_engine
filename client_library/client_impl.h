//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef UPDATE_ENGINE_CLIENT_LIBRARY_CLIENT_IMPL_H_
#define UPDATE_ENGINE_CLIENT_LIBRARY_CLIENT_IMPL_H_

#include <cstdint>
#include <memory>
#include <string>

#include <base/macros.h>

#include "update_engine/client_library/include/update_engine/client.h"
#include "update_engine/dbus-proxies.h"

namespace update_engine {
namespace internal {

class UpdateEngineClientImpl : public UpdateEngineClient {
 public:
  UpdateEngineClientImpl();
  virtual ~UpdateEngineClientImpl() = default;

  bool AttemptUpdate(const std::string& app_version,
                     const std::string& omaha_url,
                     bool at_user_request) override;

  bool GetStatus(int64_t* out_last_checked_time,
                 double* out_progress,
                 UpdateStatus* out_update_status,
                 std::string* out_new_version,
                 int64_t* out_new_size) override;

  bool SetTargetChannel(const std::string& target_channel) override;

  bool GetTargetChannel(std::string* out_channel) override;

  bool GetChannel(std::string* out_channel) override;

 private:
  std::unique_ptr<org::chromium::UpdateEngineInterfaceProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(UpdateEngineClientImpl);
};  // class UpdateEngineClientImpl

}  // namespace internal
}  // namespace update_engine

#endif  // UPDATE_ENGINE_CLIENT_LIBRARY_CLIENT_IMPL_H_
