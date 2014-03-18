// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_BZIP_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_BZIP_H_

#include <string>
#include <vector>

namespace chromeos_update_engine {

// Bzip2 compresses or decompresses str/in to out.
bool BzipDecompress(const std::vector<char>& in, std::vector<char>* out);
bool BzipCompress(const std::vector<char>& in, std::vector<char>* out);
bool BzipCompressString(const std::string& str, std::vector<char>* out);
bool BzipDecompressString(const std::string& str, std::vector<char>* out);

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_BZIP_H_
