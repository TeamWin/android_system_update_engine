// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_DOWNLOAD_ACTION_H_
#define UPDATE_ENGINE_DOWNLOAD_ACTION_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include <base/memory/scoped_ptr.h>
#include <curl/curl.h>

#include "update_engine/action.h"
#include "update_engine/delta_performer.h"
#include "update_engine/http_fetcher.h"
#include "update_engine/install_plan.h"
#include "update_engine/system_state.h"

// The Download Action downloads a specified url to disk. The url should point
// to an update in a delta payload format. The payload will be piped into a
// DeltaPerformer that will apply the delta to the disk.

namespace chromeos_update_engine {

class DownloadActionDelegate {
 public:
  // Called right before starting the download with |active| set to
  // true. Called after completing the download with |active| set to
  // false.
  virtual void SetDownloadStatus(bool active) = 0;

  // Called periodically after bytes are received. This method will be
  // invoked only if the download is active. |bytes_received| is the
  // number of bytes downloaded thus far. |total| is the number of
  // bytes expected.
  virtual void BytesReceived(uint64_t bytes_received, uint64_t total) = 0;
};

class PrefsInterface;

class DownloadAction : public InstallPlanAction,
                       public HttpFetcherDelegate {
 public:
  // Takes ownership of the passed in HttpFetcher. Useful for testing.
  // A good calling pattern is:
  // DownloadAction(prefs, system_state, new WhateverHttpFetcher);
  DownloadAction(PrefsInterface* prefs,
                 SystemState* system_state,
                 HttpFetcher* http_fetcher);
  virtual ~DownloadAction();
  void PerformAction();
  void TerminateProcessing();

  // Testing
  void SetTestFileWriter(FileWriter* writer) {
    writer_ = writer;
  }

  int GetHTTPResponseCode() { return http_fetcher_->http_response_code(); }

  // Debugging/logging
  static std::string StaticType() { return "DownloadAction"; }
  std::string Type() const { return StaticType(); }

  // HttpFetcherDelegate methods (see http_fetcher.h)
  virtual void ReceivedBytes(HttpFetcher *fetcher,
                             const char* bytes, int length);
  virtual void SeekToOffset(off_t offset);
  virtual void TransferComplete(HttpFetcher *fetcher, bool successful);
  virtual void TransferTerminated(HttpFetcher *fetcher);

  DownloadActionDelegate* delegate() const { return delegate_; }
  void set_delegate(DownloadActionDelegate* delegate) {
    delegate_ = delegate;
  }

  HttpFetcher* http_fetcher() { return http_fetcher_.get(); }

  // Returns the p2p file id for the file being written or the empty
  // string if we're not writing to a p2p file.
  std::string p2p_file_id() { return p2p_file_id_; }

 private:
  // Closes the file descriptor for the p2p file being written and
  // clears |p2p_file_id_| to indicate that we're no longer sharing
  // the file. If |delete_p2p_file| is True, also deletes the file.
  // If there is no p2p file descriptor, this method does nothing.
  void CloseP2PSharingFd(bool delete_p2p_file);

  // Starts sharing the p2p file. Must be called before
  // WriteToP2PFile(). Returns True if this worked.
  bool SetupP2PSharingFd();

  // Writes |length| bytes of payload from |data| into |file_offset|
  // of the p2p file. Also does sanity checks; for example ensures we
  // don't end up with a file with holes in it.
  //
  // This method does nothing if SetupP2PSharingFd() hasn't been
  // called or if CloseP2PSharingFd() has been called.
  void WriteToP2PFile(const char *data, size_t length, off_t file_offset);

  // The InstallPlan passed in
  InstallPlan install_plan_;

  // Update Engine preference store.
  PrefsInterface* prefs_;

  // Global context for the system.
  SystemState* system_state_;

  // Pointer to the HttpFetcher that does the http work.
  scoped_ptr<HttpFetcher> http_fetcher_;

  // The FileWriter that downloaded data should be written to. It will
  // either point to *decompressing_file_writer_ or *delta_performer_.
  FileWriter* writer_;

  scoped_ptr<DeltaPerformer> delta_performer_;

  // Used by TransferTerminated to figure if this action terminated itself or
  // was terminated by the action processor.
  ErrorCode code_;

  // For reporting status to outsiders
  DownloadActionDelegate* delegate_;
  uint64_t bytes_received_;

  // The file-id for the file we're sharing or the empty string
  // if we're not using p2p to share.
  std::string p2p_file_id_;

  // The file descriptor for the p2p file used for caching the payload or -1
  // if we're not using p2p to share.
  int p2p_sharing_fd_;

  // Set to |false| if p2p file is not visible.
  bool p2p_visible_;

  DISALLOW_COPY_AND_ASSIGN(DownloadAction);
};

// We want to be sure that we're compiled with large file support on linux,
// just in case we find ourselves downloading large images.
COMPILE_ASSERT(8 == sizeof(off_t), off_t_not_64_bit);

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DOWNLOAD_ACTION_H_
