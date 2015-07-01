// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/signature_verifier.h"

#include "omaha/base/debug.h"

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

SignatureVerifier::SignatureVerifier() : hash_object_(0), public_key_(0) {
  if (!CryptAcquireContext(provider_.receive(), NULL, NULL,
                           PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    provider_.reset();
}

SignatureVerifier::~SignatureVerifier() {
}

bool SignatureVerifier::VerifyInit(const uint8* signature_algorithm,
                                   int signature_algorithm_len,
                                   const uint8* signature,
                                   int signature_len,
                                   const uint8* public_key_info,
                                   int public_key_info_len) {
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
  if (!ok)
    return false;

  ok = CryptImportPublicKeyInfo(provider_,
                                X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                cert_public_key_info, public_key_.receive());
  free(cert_public_key_info);
  if (!ok)
    return false;

  CRYPT_ALGORITHM_IDENTIFIER* signature_algorithm_id;
  struct_len = 0;
  ok = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           X509_ALGORITHM_IDENTIFIER,
                           signature_algorithm,
                           signature_algorithm_len,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &signature_algorithm_id,
                           &struct_len);
  ASSERT1(ok || GetLastError() == ERROR_FILE_NOT_FOUND);
  ALG_ID hash_alg_id;
  if (ok) {
    hash_alg_id = CALG_MD4;  // Initialize to a weak hash algorithm that we
                             // don't support.
    if (!strcmp(signature_algorithm_id->pszObjId, szOID_RSA_SHA1RSA))
      hash_alg_id = CALG_SHA1;
    else if (!strcmp(signature_algorithm_id->pszObjId, szOID_RSA_MD5RSA))
      hash_alg_id = CALG_MD5;
    free(signature_algorithm_id);
    ASSERT1(static_cast<ALG_ID>(CALG_MD4) != hash_alg_id);
    if (hash_alg_id == CALG_MD4)
      return false;  // Unsupported hash algorithm.
  } else if (GetLastError() == ERROR_FILE_NOT_FOUND) {
    // TODO(wtc): X509_ALGORITHM_IDENTIFIER isn't supported on XP SP2.  We
    // may be able to encapsulate signature_algorithm in a dummy SignedContent
    // and decode it with X509_CERT into a CERT_SIGNED_CONTENT_INFO.  For now,
    // just hardcode the hash algorithm to be SHA-1.
    hash_alg_id = CALG_SHA1;
  } else {
    return false;
  }

  ok = CryptCreateHash(provider_, hash_alg_id, 0, 0, hash_object_.receive());
  if (!ok)
    return false;
  return true;
}

void SignatureVerifier::VerifyUpdate(const uint8* data_part,
                                     int data_part_len) {
  CryptHashData(hash_object_, data_part, data_part_len, 0);
}

bool SignatureVerifier::VerifyFinal() {
  BOOL ok = CryptVerifySignature(hash_object_, &signature_[0],
                                 static_cast<DWORD>(signature_.size()),
                                 public_key_, NULL, 0);
  Reset();
  if (!ok)
    return false;
  return true;
}

void SignatureVerifier::Reset() {
  hash_object_.reset();
  public_key_.reset();
  signature_.clear();
}

}  // namespace crypto


