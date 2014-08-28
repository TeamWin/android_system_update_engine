// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_GLIB_UTILS_H_
#define UPDATE_ENGINE_GLIB_UTILS_H_

#include <string>

#include <glib.h>

namespace chromeos_update_engine {
namespace utils {

// Returns the error message, if any, from a GError pointer. Frees the GError
// object and resets error to null.
std::string GetAndFreeGError(GError** error);

}  // namespace utils
}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_GLIB_UTILS_H_
