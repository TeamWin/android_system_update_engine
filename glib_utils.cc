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
  *error = nullptr;
  return message;
}

gchar** StringVectorToGStrv(const std::vector<string> &vec_str) {
  GPtrArray *p = g_ptr_array_new();
  for (const string& str : vec_str) {
    g_ptr_array_add(p, g_strdup(str.c_str()));
  }
  g_ptr_array_add(p, nullptr);
  return reinterpret_cast<gchar**>(g_ptr_array_free(p, FALSE));
}

}  // namespace utils
}  // namespace chromeos_update_engine
