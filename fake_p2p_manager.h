// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_P2P_MANAGER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_P2P_MANAGER_H_

#include "p2p_manager.h"

namespace chromeos_update_engine {

// A fake implementation of P2PManager.
class FakeP2PManager : public P2PManager {
public:
  FakeP2PManager() :
    is_p2p_enabled_(false),
    ensure_p2p_running_result_(false),
    ensure_p2p_not_running_result_(false),
    perform_housekeeping_result_(false),
    count_shared_files_result_(0) {}

  virtual ~FakeP2PManager() {}

  // P2PManager overrides.
  virtual void SetDevicePolicy(const policy::DevicePolicy* device_policy) {}

  virtual bool IsP2PEnabled() {
    return is_p2p_enabled_;
  }

  virtual bool EnsureP2PRunning() {
    return ensure_p2p_running_result_;
  }

  virtual bool EnsureP2PNotRunning() {
    return ensure_p2p_not_running_result_;
  }

  virtual bool PerformHousekeeping() {
    return perform_housekeeping_result_;
  }

  virtual void LookupUrlForFile(const std::string& file_id,
                                size_t minimum_size,
                                base::TimeDelta max_time_to_wait,
                                LookupCallback callback) {
    callback.Run(lookup_url_for_file_result_);
  }

  virtual bool FileShare(const std::string& file_id,
                         size_t expected_size) {
    return false;
  }

  virtual base::FilePath FileGetPath(const std::string& file_id) {
    return base::FilePath();
  }

  virtual ssize_t FileGetSize(const std::string& file_id) {
    return -1;
  }

  virtual ssize_t FileGetExpectedSize(const std::string& file_id) {
    return -1;
  }

  virtual bool FileGetVisible(const std::string& file_id,
                              bool *out_result) {
    return false;
  }

  virtual bool FileMakeVisible(const std::string& file_id) {
    return false;
  }

  virtual int CountSharedFiles() {
    return count_shared_files_result_;
  }

  // Methods for controlling what the fake returns and how it acts.
  void SetP2PEnabled(bool is_p2p_enabled) {
    is_p2p_enabled_ = is_p2p_enabled;
  }

  void SetEnsureP2PRunningResult(bool ensure_p2p_running_result) {
    ensure_p2p_running_result_ = ensure_p2p_running_result;
  }

  void SetEnsureP2PNotRunningResult(bool ensure_p2p_not_running_result) {
    ensure_p2p_not_running_result_ = ensure_p2p_not_running_result;
  }

  void SetPerformHousekeepingResult(bool perform_housekeeping_result) {
    perform_housekeeping_result_ = perform_housekeeping_result;
  }

  void SetCountSharedFilesResult(int count_shared_files_result) {
    count_shared_files_result_ = count_shared_files_result;
  }

  void SetLookupUrlForFileResult(const std::string& url) {
    lookup_url_for_file_result_ = url;
  }

private:
  bool is_p2p_enabled_;
  bool ensure_p2p_running_result_;
  bool ensure_p2p_not_running_result_;
  bool perform_housekeeping_result_;
  int count_shared_files_result_;
  std::string lookup_url_for_file_result_;

  DISALLOW_COPY_AND_ASSIGN(FakeP2PManager);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FAKE_P2P_MANAGER_H_
