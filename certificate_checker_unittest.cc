// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/certificate_checker.h"

#include <string>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <metrics/metrics_library_mock.h>

#include "update_engine/constants.h"
#include "update_engine/fake_system_state.h"
#include "update_engine/mock_certificate_checker.h"
#include "update_engine/mock_prefs.h"

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::SetArrayArgument;
using ::testing::_;
using std::string;

namespace chromeos_update_engine {

class CertificateCheckerTest : public testing::Test {
 public:
  CertificateCheckerTest() {}

 protected:
  void SetUp() override {
    depth_ = 0;
    length_ = 4;
    digest_[0] = 0x17;
    digest_[1] = 0x7D;
    digest_[2] = 0x07;
    digest_[3] = 0x5F;
    digest_hex_ = "177D075F";
    diff_digest_hex_ = "1234ABCD";
    cert_key_prefix_ = kPrefsUpdateServerCertificate;
    server_to_check_ = CertificateChecker::kUpdate;
    cert_key_ = base::StringPrintf("%s-%d-%d",
                                   cert_key_prefix_.c_str(),
                                   server_to_check_,
                                   depth_);
    kCertChanged = "Updater.ServerCertificateChanged";
    kCertFailed = "Updater.ServerCertificateFailed";
    CertificateChecker::set_system_state(&fake_system_state_);
    CertificateChecker::set_openssl_wrapper(&openssl_wrapper_);
    prefs_ = fake_system_state_.mock_prefs();
  }

  void TearDown() override {}

  FakeSystemState fake_system_state_;
  MockPrefs* prefs_;  // shortcut to fake_system_state_.mock_prefs()
  MockOpenSSLWrapper openssl_wrapper_;
  // Parameters of our mock certificate digest.
  int depth_;
  unsigned int length_;
  uint8_t digest_[4];
  string digest_hex_;
  string diff_digest_hex_;
  string cert_key_prefix_;
  CertificateChecker::ServerToCheck server_to_check_;
  string cert_key_;
  string kCertChanged;
  string kCertFailed;
};

// check certificate change, new
TEST_F(CertificateCheckerTest, NewCertificate) {
  EXPECT_CALL(openssl_wrapper_, GetCertificateDigest(nullptr, _, _, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(depth_),
          SetArgumentPointee<2>(length_),
          SetArrayArgument<3>(digest_, digest_ + 4),
          Return(true)));
  EXPECT_CALL(*prefs_, GetString(cert_key_, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*prefs_, SetString(cert_key_, digest_hex_))
      .WillOnce(Return(true));
  ASSERT_TRUE(CertificateChecker::CheckCertificateChange(
      server_to_check_, 1, nullptr));
}

// check certificate change, unchanged
TEST_F(CertificateCheckerTest, SameCertificate) {
  EXPECT_CALL(openssl_wrapper_, GetCertificateDigest(nullptr, _, _, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(depth_),
          SetArgumentPointee<2>(length_),
          SetArrayArgument<3>(digest_, digest_ + 4),
          Return(true)));
  EXPECT_CALL(*prefs_, GetString(cert_key_, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(digest_hex_),
          Return(true)));
  EXPECT_CALL(*prefs_, SetString(_, _)).Times(0);
  ASSERT_TRUE(CertificateChecker::CheckCertificateChange(
      server_to_check_, 1, nullptr));
}

// check certificate change, changed
TEST_F(CertificateCheckerTest, ChangedCertificate) {
  EXPECT_CALL(openssl_wrapper_, GetCertificateDigest(nullptr, _, _, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(depth_),
          SetArgumentPointee<2>(length_),
          SetArrayArgument<3>(digest_, digest_ + 4),
          Return(true)));
  EXPECT_CALL(*prefs_, GetString(cert_key_, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(diff_digest_hex_),
          Return(true)));
  EXPECT_CALL(*prefs_, SetString(kPrefsCertificateReportToSendUpdate,
                                kCertChanged))
      .WillOnce(Return(true));
  EXPECT_CALL(*prefs_, SetString(cert_key_, digest_hex_))
      .WillOnce(Return(true));
  ASSERT_TRUE(CertificateChecker::CheckCertificateChange(
      server_to_check_, 1, nullptr));
}

// check certificate change, failed
TEST_F(CertificateCheckerTest, FailedCertificate) {
  EXPECT_CALL(*prefs_, SetString(kPrefsCertificateReportToSendUpdate,
                                kCertFailed))
      .WillOnce(Return(true));
  EXPECT_CALL(*prefs_, GetString(_, _)).Times(0);
  EXPECT_CALL(openssl_wrapper_, GetCertificateDigest(_, _, _, _)).Times(0);
  ASSERT_FALSE(CertificateChecker::CheckCertificateChange(
      server_to_check_, 0, nullptr));
}

// flush send report
TEST_F(CertificateCheckerTest, FlushReport) {
  EXPECT_CALL(*prefs_, GetString(kPrefsCertificateReportToSendUpdate, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(kCertChanged),
          Return(true)));
  EXPECT_CALL(*prefs_, GetString(kPrefsCertificateReportToSendDownload, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*fake_system_state_.mock_metrics_lib(),
              SendUserActionToUMA(kCertChanged))
      .WillOnce(Return(true));
  EXPECT_CALL(*prefs_, Delete(kPrefsCertificateReportToSendUpdate))
      .WillOnce(Return(true));
  EXPECT_CALL(*prefs_, SetString(kPrefsCertificateReportToSendDownload, _))
      .Times(0);
  CertificateChecker::FlushReport();
}

// flush nothing to report
TEST_F(CertificateCheckerTest, FlushNothingToReport) {
  string empty = "";
  EXPECT_CALL(*prefs_, GetString(kPrefsCertificateReportToSendUpdate, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(empty),
          Return(true)));
  EXPECT_CALL(*prefs_, GetString(kPrefsCertificateReportToSendDownload, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*fake_system_state_.mock_metrics_lib(),
              SendUserActionToUMA(_)).Times(0);
  EXPECT_CALL(*prefs_, SetString(_, _)).Times(0);
  CertificateChecker::FlushReport();
}

}  // namespace chromeos_update_engine
