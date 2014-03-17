// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/policy_manager/event_loop.h"

#include <cmath>

using base::Closure;

namespace {

// Called by the GLib's main loop when is time to call the callback scheduled
// with RunFromMainLopp() and similar functions. The pointer to the callback
// passed when scheduling it is passed to this functions as a gpointer on
// |user_data|.
gboolean OnRanFromMainLoop(gpointer user_data) {
  Closure* callback_p = reinterpret_cast<Closure*>(user_data);
  callback_p->Run();
  delete callback_p;
  return FALSE;  // Removes the source since a callback can only be called once.
}

}  // namespace

namespace chromeos_policy_manager {

EventId RunFromMainLoop(const Closure& callback) {
  Closure* callback_p = new Closure(callback);
  return g_idle_add_full(G_PRIORITY_DEFAULT,
                         OnRanFromMainLoop,
                         reinterpret_cast<gpointer>(callback_p),
                         NULL);
}

EventId RunFromMainLoopAfterTimeout(
    const Closure& callback,
    base::TimeDelta timeout) {
  Closure* callback_p = new Closure(callback);
  return g_timeout_add_seconds(static_cast<guint>(ceil(timeout.InSecondsF())),
                               OnRanFromMainLoop,
                               reinterpret_cast<gpointer>(callback_p));
}

bool CancelMainLoopEvent(EventId event) {
  if (event != kEventIdNull)
    return g_source_remove(event);
  return false;
}

}  // namespace chromeos_policy_manager
