// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/signature_verifier_win.h"

#include "crypto/rsa_private_key.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"

namespace {

// Wrappers of malloc and free for CRYPT_DECODE_PARA, which requires the
// WINAPI calling convention.
void* WINAPI MyCryptAlloc(size_t size) {
  return malloc(size);
}

void WINAPI MyCryptFree(void* p) {
  free(p);
}

}  // namespace

namespace crypto {

SignatureVerifierWin::SignatureVerifierWin() : hash_object_(0), public_key_(0) {
  if (FAILED(CryptAcquireContextWithFallback(PROV_RSA_AES,
                                             provider_.receive()))) {
    provider_.reset();
  }
}

SignatureVerifierWin::~SignatureVerifierWin() {
}

bool SignatureVerifierWin::VerifyInit(ALG_ID algorithm_id,
                                      const uint8_t* signature,
                                      size_t signature_len,
                                      const uint8_t* public_key_info,
                                      size_t public_key_info_len) {
  if (algorithm_id != CALG_SHA_256 && algorithm_id != CALG_SHA1) {
    REPORT_LOG(LE, (_T("[VerifyInit][Invalid signature algorithm][%d]"),
                    algorithm_id));
    return false;
  }

  signature_.reserve(signature_len);
  // CryptoAPI uses big integers in the little-endian byte order, so we need
  // to first swap the order of signature bytes.
  for (int i = signature_len - 1; i >= 0; --i)
    signature_.push_back(signature[i]);

  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = MyCryptAlloc;
  decode_para.pfnFree = MyCryptFree;
  CERT_PUBLIC_KEY_INFO* cert_public_key_info = NULL;
  DWORD struct_len = 0;
  BOOL ok;
  ok = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           X509_PUBLIC_KEY_INFO,
                           public_key_info,
                           public_key_info_len,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &cert_public_key_info,
                           &struct_len);
  if (!ok) {
    HRESULT hr = omaha::HRESULTFromLastError();
    REPORT_LOG(LE, (_T("[VerifyInit][CryptDecodeObjectEx failed][%#x]"), hr));
    return false;
  }

  ok = CryptImportPublicKeyInfo(provider_,
                                X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                cert_public_key_info, public_key_.receive());
  free(cert_public_key_info);
  if (!ok) {
    HRESULT hr = omaha::HRESULTFromLastError();
    REPORT_LOG(LE, (_T("[VerifyInit][CryptImportPublicKeyInfo failed][%#x]"),
                    hr));
    return false;
  }

  ok = CryptCreateHash(provider_, algorithm_id, 0, 0, hash_object_.receive());
  if (!ok) {
    HRESULT hr = omaha::HRESULTFromLastError();
    REPORT_LOG(LE, (_T("[VerifyFinal][CryptVerifySignature failed][%#x]"), hr));
    return false;
  }

  return true;
}

void SignatureVerifierWin::VerifyUpdate(const uint8_t* data_part,
                                        size_t data_part_len) {
  BOOL ok = CryptHashData(hash_object_, data_part, data_part_len, 0);
  if (!ok) {
    HRESULT hr = omaha::HRESULTFromLastError();
    REPORT_LOG(LE, (_T("[VerifyUpdate][CryptHashData failed][%#x]"), hr));
  }
}

bool SignatureVerifierWin::VerifyFinal() {
  BOOL ok = CryptVerifySignature(hash_object_, &signature_[0],
                                 static_cast<DWORD>(signature_.size()),
                                 public_key_, NULL, 0);
  Reset();
  if (!ok) {
    HRESULT hr = omaha::HRESULTFromLastError();
    REPORT_LOG(LE, (_T("[VerifyFinal][CryptVerifySignature failed][%#x]"), hr));
    return false;
  }

  return true;
}

void SignatureVerifierWin::Reset() {
  hash_object_.reset();
  public_key_.reset();
  signature_.clear();
}

}  // namespace crypto


