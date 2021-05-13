//
// Copyright (C) 2021 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_SCOPED_TASK_ID_H_
#define UPDATE_ENGINE_SCOPED_TASK_ID_H_

#include <type_traits>
#include <utility>

#include <base/bind.h>
#include <brillo/message_loops/message_loop.h>

namespace chromeos_update_engine {

// This class provides unique_ptr like semantic for |MessageLoop::TaskId|, when
// instance of this class goes out of scope, underlying task will be cancelled.
class ScopedTaskId {
  using MessageLoop = brillo::MessageLoop;

 public:
  constexpr ScopedTaskId() = default;

  constexpr ScopedTaskId(ScopedTaskId&& other) noexcept {
    *this = std::move(other);
  }

  constexpr ScopedTaskId& operator=(ScopedTaskId&& other) noexcept {
    std::swap(task_id_, other.task_id_);
    return *this;
  }

  // Post a callback on current message loop, return true if succeeded, false if
  // the previous callback hasn't run yet, or scheduling failed at MessageLoop
  // side.
  [[nodiscard]] bool PostTask(const base::Location& from_here,
                              base::OnceClosure&& callback,
                              base::TimeDelta delay = {}) noexcept {
    return PostTask<decltype(callback)>(from_here, std::move(callback), delay);
  }
  [[nodiscard]] bool PostTask(const base::Location& from_here,
                              std::function<void()>&& callback,
                              base::TimeDelta delay = {}) noexcept {
    return PostTask<decltype(callback)>(from_here, std::move(callback), delay);
  }

  ~ScopedTaskId() noexcept { Cancel(); }

  // Cancel the underlying managed task, true if cancel successful. False if no
  // task scheduled or task cancellation failed
  bool Cancel() noexcept {
    if (task_id_ != MessageLoop::kTaskIdNull) {
      if (MessageLoop::current()->CancelTask(task_id_)) {
        LOG(INFO) << "Cancelled task id " << task_id_;
        task_id_ = MessageLoop::kTaskIdNull;
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] constexpr bool IsScheduled() const noexcept {
    return task_id_ != MessageLoop::kTaskIdNull;
  }

  [[nodiscard]] constexpr bool operator==(const ScopedTaskId& other) const
      noexcept {
    return other.task_id_ == task_id_;
  }

  [[nodiscard]] constexpr bool operator<(const ScopedTaskId& other) const
      noexcept {
    return task_id_ < other.task_id_;
  }

 private:
  ScopedTaskId(const ScopedTaskId&) = delete;
  ScopedTaskId& operator=(const ScopedTaskId&) = delete;
  template <typename Callable>
  [[nodiscard]] bool PostTask(const base::Location& from_here,
                              Callable&& callback,
                              base::TimeDelta delay) noexcept {
    if (task_id_ != MessageLoop::kTaskIdNull) {
      LOG(ERROR) << "Scheduling another task but task id " << task_id_
                 << " isn't executed yet! This can cause the old task to leak.";
      return false;
    }
    task_id_ = MessageLoop::current()->PostDelayedTask(
        from_here,
        base::BindOnce(&ScopedTaskId::ExecuteTask<decltype(callback)>,
                       base::Unretained(this),
                       std::move(callback)),
        delay);
    return task_id_ != MessageLoop::kTaskIdNull;
  }
  template <typename Callable>
  void ExecuteTask(Callable&& callback) {
    task_id_ = MessageLoop::kTaskIdNull;
    if constexpr (std::is_same_v<Callable&&, base::OnceClosure&&>) {
      std::move(callback).Run();
    } else {
      std::move(callback)();
    }
  }
  MessageLoop::TaskId task_id_{MessageLoop::kTaskIdNull};
};
}  // namespace chromeos_update_engine

#endif
