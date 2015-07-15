// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_BZIP_H_
#define UPDATE_ENGINE_BZIP_H_

#include <string>
#include <vector>

#include <chromeos/secure_blob.h>

namespace chromeos_update_engine {

// Bzip2 compresses or decompresses str/in to out.
bool BzipDecompress(const chromeos::Blob& in, chromeos::Blob* out);
bool BzipCompress(const chromeos::Blob& in, chromeos::Blob* out);
bool BzipCompressString(const std::string& str, chromeos::Blob* out);
bool BzipDecompressString(const std::string& str, chromeos::Blob* out);

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_BZIP_H_
