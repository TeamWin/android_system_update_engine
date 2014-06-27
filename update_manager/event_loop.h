// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(deymo): These functions interact with the glib's main loop. This should
// be replaced by the libbase main loop once the process is migrated to that
// main loop.

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_EVENT_LOOP_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_EVENT_LOOP_H_

#include <glib.h>

#include <base/callback.h>
#include <base/time/time.h>

namespace chromeos_update_manager {

typedef guint EventId;

// A null EventId doesn't idenify any valid event.
static constexpr EventId kEventIdNull = 0;

// Schedules the passed |callback| to run from the GLib's main loop after a
// timeout if it is given.
EventId RunFromMainLoop(const base::Closure& callback);
EventId RunFromMainLoopAfterTimeout(const base::Closure& callback,
                                    base::TimeDelta timeout);

// Removes the pending call |event| from the main loop. The value passed is the
// one returned by the functions RunFromMainLoop*() when the call was scheduled.
// Returns whether the event was found and removed.
bool CancelMainLoopEvent(EventId event);

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_EVENT_LOOP_H_
