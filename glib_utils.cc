// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/glib_utils.h"

#include <base/strings/stringprintf.h>

using std::string;

namespace chromeos_update_engine {
namespace utils {

string GetAndFreeGError(GError** error) {
  if (!*error) {
    return "Unknown GLib error.";
  }
  string message =
      base::StringPrintf("GError(%d): %s",
                         (*error)->code,
                         (*error)->message ? (*error)->message : "(unknown)");
  g_error_free(*error);
  *error = NULL;
  return message;
}

}  // namespace utils
}  // namespace chromeos_update_engine
