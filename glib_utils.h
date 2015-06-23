// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_GLIB_UTILS_H_
#define UPDATE_ENGINE_GLIB_UTILS_H_

#include <string>
#include <vector>

#include <glib.h>

namespace chromeos_update_engine {
namespace utils {

// Returns the error message, if any, from a GError pointer. Frees the GError
// object and resets error to null.
std::string GetAndFreeGError(GError** error);

// Converts a vector of strings to a NUL-terminated array of
// strings. The resulting array should be freed with g_strfreev()
// when are you done with it.
gchar** StringVectorToGStrv(const std::vector<std::string>& vec_str);

// A base::FreeDeleter that frees memory using g_free(). Useful when
// integrating with GLib since it can be used with std::unique_ptr to
// automatically free memory when going out of scope.
struct GLibFreeDeleter {
  inline void operator()(void* ptr) const {
    g_free(static_cast<gpointer>(ptr));
  }
};

// A base::FreeDeleter that frees memory using g_strfreev(). Useful
// when integrating with GLib since it can be used with std::unique_ptr to
// automatically free memory when going out of scope.
struct GLibStrvFreeDeleter {
  inline void operator()(void* ptr) const {
    g_strfreev(static_cast<gchar**>(ptr));
  }
};

}  // namespace utils
}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_GLIB_UTILS_H_
