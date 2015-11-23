//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/common/certificate_checker.h"

#include <string>

#include <base/lazy_instance.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/thread_local.h>
#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>

#include "update_engine/common/constants.h"
#include "update_engine/common/prefs_interface.h"
#include "update_engine/common/utils.h"

using std::string;

namespace chromeos_update_engine {

namespace {
// A lazily created thread local storage for passing the current certificate
// checker to the openssl callback.
base::LazyInstance<base::ThreadLocalPointer<CertificateChecker>>::Leaky
    lazy_tls_ptr;
}  // namespace

bool OpenSSLWrapper::GetCertificateDigest(X509_STORE_CTX* x509_ctx,
                                          int* out_depth,
                                          unsigned int* out_digest_length,
                                          uint8_t* out_digest) const {
  TEST_AND_RETURN_FALSE(out_digest);
  X509* certificate = X509_STORE_CTX_get_current_cert(x509_ctx);
  TEST_AND_RETURN_FALSE(certificate);
  int depth = X509_STORE_CTX_get_error_depth(x509_ctx);
  if (out_depth)
    *out_depth = depth;

  unsigned int len;
  const EVP_MD* digest_function = EVP_sha256();
  bool success = X509_digest(certificate, digest_function, out_digest, &len);

  if (success && out_digest_length)
    *out_digest_length = len;
  return success;
}

CertificateChecker::CertificateChecker(PrefsInterface* prefs,
                                       OpenSSLWrapper* openssl_wrapper,
                                       ServerToCheck server_to_check)
    : prefs_(prefs),
      openssl_wrapper_(openssl_wrapper),
      server_to_check_(server_to_check) {
}

// static
CURLcode CertificateChecker::ProcessSSLContext(CURL* curl_handle,
                                               SSL_CTX* ssl_ctx,
                                               void* ptr) {
  CertificateChecker* cert_checker = reinterpret_cast<CertificateChecker*>(ptr);

  // From here we set the SSL_CTX to another callback, from the openssl library,
  // which will be called after each server certificate is validated. However,
  // since openssl does not allow us to pass our own data pointer to the
  // callback, the certificate check will have to be done statically. To pass
  // the pointer to this instance, we use a thread-safe pointer in lazy_tls_ptr
  // during the calls and clear them after it.
  CHECK(cert_checker != nullptr);
  lazy_tls_ptr.Pointer()->Set(cert_checker);
  SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, VerifySSLCallback);
  // Sanity check: We should not re-enter this method during certificate
  // checking.
  CHECK(lazy_tls_ptr.Pointer()->Get() == cert_checker);
  lazy_tls_ptr.Pointer()->Set(nullptr);

  return CURLE_OK;
}

// static
int CertificateChecker::VerifySSLCallback(int preverify_ok,
                                          X509_STORE_CTX* x509_ctx) {
  CertificateChecker* cert_checker = lazy_tls_ptr.Pointer()->Get();
  CHECK(cert_checker != nullptr);
  return cert_checker->CheckCertificateChange(preverify_ok, x509_ctx) ? 1 : 0;
}

bool CertificateChecker::CheckCertificateChange(int preverify_ok,
                                                X509_STORE_CTX* x509_ctx) {
  TEST_AND_RETURN_FALSE(prefs_ != nullptr);

  // If pre-verification failed, we are not interested in the current
  // certificate. We store a report to UMA and just propagate the fail result.
  if (!preverify_ok) {
    NotifyCertificateChecked(CertificateCheckResult::kFailed);
    return false;
  }

  int depth;
  unsigned int digest_length;
  uint8_t digest[EVP_MAX_MD_SIZE];

  if (!openssl_wrapper_->GetCertificateDigest(x509_ctx,
                                              &depth,
                                              &digest_length,
                                              digest)) {
    LOG(WARNING) << "Failed to generate digest of X509 certificate "
                 << "from update server.";
    NotifyCertificateChecked(CertificateCheckResult::kValid);
    return true;
  }

  // We convert the raw bytes of the digest to an hex string, for storage in
  // prefs.
  string digest_string = base::HexEncode(digest, digest_length);

  string storage_key =
      base::StringPrintf("%s-%d-%d", kPrefsUpdateServerCertificate,
                         static_cast<int>(server_to_check_), depth);
  string stored_digest;
  // If there's no stored certificate, we just store the current one and return.
  if (!prefs_->GetString(storage_key, &stored_digest)) {
    if (!prefs_->SetString(storage_key, digest_string)) {
      LOG(WARNING) << "Failed to store server certificate on storage key "
                   << storage_key;
    }
    NotifyCertificateChecked(CertificateCheckResult::kValid);
    return true;
  }

  // Certificate changed, we store a report to UMA and store the most recent
  // certificate.
  if (stored_digest != digest_string) {
    if (!prefs_->SetString(storage_key, digest_string)) {
      LOG(WARNING) << "Failed to store server certificate on storage key "
                   << storage_key;
    }
    NotifyCertificateChecked(CertificateCheckResult::kValidChanged);
    return true;
  }

  NotifyCertificateChecked(CertificateCheckResult::kValid);
  // Since we don't perform actual SSL verification, we return success.
  return true;
}

void CertificateChecker::NotifyCertificateChecked(
    CertificateCheckResult result) {
  if (observer_)
    observer_->CertificateChecked(server_to_check_, result);
}

}  // namespace chromeos_update_engine
