// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_manager/event_loop.h"

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
  return FALSE;  // Removes the source since a callback can only be called once.
}

void DestroyClosure(gpointer user_data) {
  delete reinterpret_cast<Closure*>(user_data);
}

}  // namespace

namespace chromeos_update_manager {

EventId RunFromMainLoop(const Closure& callback) {
  Closure* callback_p = new Closure(callback);
  return g_idle_add_full(G_PRIORITY_DEFAULT,
                         OnRanFromMainLoop,
                         reinterpret_cast<gpointer>(callback_p),
                         DestroyClosure);
}

EventId RunFromMainLoopAfterTimeout(
    const Closure& callback,
    base::TimeDelta timeout) {
  Closure* callback_p = new Closure(callback);
  return g_timeout_add_seconds_full(
      G_PRIORITY_DEFAULT,
      static_cast<guint>(ceil(timeout.InSecondsF())),
      OnRanFromMainLoop,
      reinterpret_cast<gpointer>(callback_p),
      DestroyClosure);
}

bool CancelMainLoopEvent(EventId event) {
  if (event != kEventIdNull)
    return g_source_remove(event);
  return false;
}

}  // namespace chromeos_update_manager
