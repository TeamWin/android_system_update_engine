// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_FILESYSTEM_VERIFIER_ACTION_H_
#define UPDATE_ENGINE_FILESYSTEM_VERIFIER_ACTION_H_

#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include <gio/gio.h>
#include <glib.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/action.h"
#include "update_engine/install_plan.h"
#include "update_engine/omaha_hash_calculator.h"

// This action will only do real work if it's a delta update. It will
// copy the root partition to install partition, and then terminate.

namespace chromeos_update_engine {

class SystemState;

// The type of filesystem that we are verifying.
enum class PartitionType {
  kSourceRootfs,
  kSourceKernel,
  kRootfs,
  kKernel,
};

class FilesystemVerifierAction : public InstallPlanAction {
 public:
  FilesystemVerifierAction(SystemState* system_state,
                           PartitionType partition_type);

  void PerformAction() override;
  void TerminateProcessing() override;

  // Used for testing. Return true if Cleanup() has not yet been called due
  // to a callback upon the completion or cancellation of the verifier action.
  // A test should wait until IsCleanupPending() returns false before
  // terminating the glib main loop.
  bool IsCleanupPending() const;

  // Debugging/logging
  static std::string StaticType() { return "FilesystemVerifierAction"; }
  std::string Type() const override { return StaticType(); }

 private:
  friend class FilesystemVerifierActionTest;
  FRIEND_TEST(FilesystemVerifierActionTest,
              RunAsRootDetermineFilesystemSizeTest);

  // Callbacks from glib when the read operation is done.
  void AsyncReadReadyCallback(GObject *source_object, GAsyncResult *res);
  static void StaticAsyncReadReadyCallback(GObject *source_object,
                                           GAsyncResult *res,
                                           gpointer user_data);

  // Based on the state of the ping-pong buffers spawns appropriate read
  // actions asynchronously.
  void SpawnAsyncActions();

  // Cleans up all the variables we use for async operations and tells the
  // ActionProcessor we're done w/ |code| as passed in. |cancelled_| should be
  // true if TerminateProcessing() was called.
  void Cleanup(ErrorCode code);

  // Determine, if possible, the source file system size to avoid copying the
  // whole partition. Currently this supports only the root file system assuming
  // it's ext3-compatible.
  void DetermineFilesystemSize(int fd);

  // The type of the partition that we are verifying.
  PartitionType partition_type_;

  // If non-null, this is the GUnixInputStream object for the opened source
  // partition.
  GInputStream* src_stream_;

  // Buffer for storing data we read.
  chromeos::Blob buffer_;

  // The cancellable object for the in-flight async calls.
  GCancellable* canceller_;

  bool read_done_;  // true if reached EOF on the input stream.
  bool failed_;  // true if the action has failed.
  bool cancelled_;  // true if the action has been cancelled.

  // The install plan we're passed in via the input pipe.
  InstallPlan install_plan_;

  // Calculates the hash of the data.
  OmahaHashCalculator hasher_;

  // Reads and hashes this many bytes from the head of the input stream. This
  // field is initialized when the action is started and decremented as more
  // bytes get read.
  int64_t remaining_size_;

  // The global context for update_engine.
  SystemState* system_state_;

  DISALLOW_COPY_AND_ASSIGN(FilesystemVerifierAction);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_FILESYSTEM_VERIFIER_ACTION_H_
