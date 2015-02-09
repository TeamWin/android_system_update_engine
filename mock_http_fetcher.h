// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_MOCK_HTTP_FETCHER_H_
#define UPDATE_ENGINE_MOCK_HTTP_FETCHER_H_

#include <string>
#include <vector>

#include <base/logging.h>
#include <glib.h>

#include "update_engine/fake_system_state.h"
#include "update_engine/http_fetcher.h"
#include "update_engine/mock_connection_manager.h"

// This is a mock implementation of HttpFetcher which is useful for testing.
// All data must be passed into the ctor. When started, MockHttpFetcher will
// deliver the data in chunks of size kMockHttpFetcherChunkSize. To simulate
// a network failure, you can call FailTransfer().

namespace chromeos_update_engine {

// MockHttpFetcher will send a chunk of data down in each call to BeginTransfer
// and Unpause. For the other chunks of data, a callback is put on the run
// loop and when that's called, another chunk is sent down.
const size_t kMockHttpFetcherChunkSize(65536);

class MockHttpFetcher : public HttpFetcher {
 public:
  // The data passed in here is copied and then passed to the delegate after
  // the transfer begins.
  MockHttpFetcher(const uint8_t* data,
                  size_t size,
                  ProxyResolver* proxy_resolver)
      : HttpFetcher(proxy_resolver, &fake_system_state_),
        sent_size_(0),
        timeout_source_(nullptr),
        timout_tag_(0),
        paused_(false),
        fail_transfer_(false),
        never_use_(false),
        mock_connection_manager_(&fake_system_state_) {
    fake_system_state_.set_connection_manager(&mock_connection_manager_);
    data_.insert(data_.end(), data, data + size);
  }

  // Constructor overload for string data.
  MockHttpFetcher(const char* data, size_t size, ProxyResolver* proxy_resolver)
      : MockHttpFetcher(reinterpret_cast<const uint8_t*>(data), size,
                        proxy_resolver) {}

  // Cleans up all internal state. Does not notify delegate
  ~MockHttpFetcher() override;

  // Ignores this.
  void SetOffset(off_t offset) override {
    sent_size_ = offset;
    if (delegate_)
      delegate_->SeekToOffset(offset);
  }

  // Do nothing.
  void SetLength(size_t length) override {}
  void UnsetLength() override {}
  void set_low_speed_limit(int low_speed_bps, int low_speed_sec) override {}
  void set_connect_timeout(int connect_timeout_seconds) override {}
  void set_max_retry_count(int max_retry_count) override {}

  // Dummy: no bytes were downloaded.
  size_t GetBytesDownloaded() override {
    return sent_size_;
  }

  // Begins the transfer if it hasn't already begun.
  void BeginTransfer(const std::string& url) override;

  // If the transfer is in progress, aborts the transfer early.
  // The transfer cannot be resumed.
  void TerminateTransfer() override;

  // Suspend the mock transfer.
  void Pause() override;

  // Resume the mock transfer.
  void Unpause() override;

  // Fail the transfer. This simulates a network failure.
  void FailTransfer(int http_response_code);

  // If set to true, this will EXPECT fail on BeginTransfer
  void set_never_use(bool never_use) { never_use_ = never_use; }

  const chromeos::Blob& post_data() const {
    return post_data_;
  }

 private:
  // Sends data to the delegate and sets up a glib timeout callback if needed.
  // There must be a delegate and there must be data to send. If there is
  // already a timeout callback, and it should be deleted by the caller,
  // this will return false; otherwise true is returned.
  // If skip_delivery is true, no bytes will be delivered, but the callbacks
  // still be set if needed.
  bool SendData(bool skip_delivery);

  // Callback for when our glib main loop callback is called
  bool TimeoutCallback();
  static gboolean StaticTimeoutCallback(gpointer data) {
    return reinterpret_cast<MockHttpFetcher*>(data)->TimeoutCallback();
  }

  // Sets the HTTP response code and signals to the delegate that the transfer
  // is complete.
  void SignalTransferComplete();

  // A full copy of the data we'll return to the delegate
  chromeos::Blob data_;

  // The number of bytes we've sent so far
  size_t sent_size_;

  // The glib main loop timeout source. After each chunk of data sent, we
  // time out for 0s just to make sure that run loop services other clients.
  GSource* timeout_source_;

  // ID of the timeout source, valid only if timeout_source_ != null
  guint timout_tag_;

  // True iff the fetcher is paused.
  bool paused_;

  // Set to true if the transfer should fail.
  bool fail_transfer_;

  // Set to true if BeginTransfer should EXPECT fail.
  bool never_use_;

  FakeSystemState fake_system_state_;
  MockConnectionManager mock_connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(MockHttpFetcher);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_MOCK_HTTP_FETCHER_H_
