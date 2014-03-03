// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/policy_manager.h"

#include "update_engine/policy_manager/chromeos_policy.h"
#include "update_engine/policy_manager/real_state.h"

using base::Closure;

namespace chromeos_policy_manager {

template <typename T>
bool InitProvider(scoped_ptr<T>* handle_ptr, T* provider) {
   handle_ptr->reset(provider);
   return handle_ptr->get() && (*handle_ptr)->Init();
}

bool PolicyManager::Init(chromeos_update_engine::DBusWrapperInterface* dbus,
                         chromeos_update_engine::ClockInterface* clock) {
  // TODO(deymo): Make it possible to replace this policy with a different
  // implementation with a build-time flag.
  policy_.reset(new ChromeOSPolicy());

  state_.reset(new RealState(dbus, clock));

  return state_->Init();
}

void PolicyManager::RunFromMainLoop(const Closure& callback) {
  Closure* callback_p = new base::Closure(callback);
  g_idle_add_full(G_PRIORITY_DEFAULT,
                  OnRanFromMainLoop,
                  reinterpret_cast<gpointer>(callback_p),
                  NULL);
}

void PolicyManager::RunFromMainLoopAfterTimeout(
    const Closure& callback,
    base::TimeDelta timeout) {
  Closure* callback_p = new Closure(callback);
  g_timeout_add_seconds(timeout.InSeconds(),
                        OnRanFromMainLoop,
                        reinterpret_cast<gpointer>(callback_p));
}

gboolean PolicyManager::OnRanFromMainLoop(gpointer user_data) {
  Closure* callback_p = reinterpret_cast<Closure*>(user_data);
  callback_p->Run();
  delete callback_p;
  return FALSE;  // Removes the source since a callback can only be called once.
}

}  // namespace chromeos_policy_manager
