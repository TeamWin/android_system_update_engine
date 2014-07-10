// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_FILE_WRITER_MOCK_H_
#define UPDATE_ENGINE_FILE_WRITER_MOCK_H_

#include "gmock/gmock.h"
#include "update_engine/file_writer.h"

namespace chromeos_update_engine {

class FileWriterMock : public FileWriter {
 public:
  MOCK_METHOD3(Open, int(const char* path, int flags, mode_t mode));
  MOCK_METHOD2(Write, ssize_t(const void* bytes, size_t count));
  MOCK_METHOD0(Close, int());
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_FILE_WRITER_MOCK_H_
