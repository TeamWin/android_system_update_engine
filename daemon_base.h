//
// Copyright (C) 2019 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_DAEMON_BASE_H_
#define UPDATE_ENGINE_DAEMON_BASE_H_

#include <memory>

#include <brillo/daemons/daemon.h>

namespace chromeos_update_engine {

class DaemonBase : public brillo::Daemon {
 public:
  DaemonBase() = default;
  virtual ~DaemonBase() = default;

  // Creates an instance of the daemon.
  static std::unique_ptr<DaemonBase> CreateInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(DaemonBase);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DAEMON_BASE_H_
