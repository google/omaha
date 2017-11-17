// Copyright 2017 Google Inc.
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
// ========================================================================
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_SIGNATURE_VERIFIER_H_
#define OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_SIGNATURE_VERIFIER_H_

#include <stdint.h>
#include <memory>
#include <vector>
#include "base/scoped_ptr.h"
#include "crypto/crypto_export.h"
#include "omaha/base/signatures.h"
#include "omaha/net/cup_ecdsa_utils.h"

typedef struct env_md_st EVP_MD;
typedef struct evp_pkey_ctx_st EVP_PKEY_CTX;

namespace crypto {

// The SignatureVerifier class verifies a signature using a bare public key
// (as opposed to a certificate).
class CRYPTO_EXPORT SignatureVerifier {
 public:
  // The set of supported hash functions. Extend as required.
  enum HashAlgorithm {
    SHA256,
  };

  // The set of supported signature algorithms. Extend as required.
  enum SignatureAlgorithm {
    RSA_PKCS1_SHA256,
    ECDSA_SHA256,
  };

  SignatureVerifier();
  ~SignatureVerifier();

  // Streaming interface:

  // Initiates a signature verification operation.  This should be followed
  // by one or more VerifyUpdate calls and a VerifyFinal call.
  //
  // The signature is encoded according to the signature algorithm.
  //
  // The public key is specified as a DER encoded ASN.1 SubjectPublicKeyInfo
  // structure, which contains not only the public key but also its type
  // (algorithm):
  //   SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //       algorithm            AlgorithmIdentifier,
  //       subjectPublicKey     BIT STRING  }
  bool VerifyInit(SignatureAlgorithm signature_algorithm,
                  const uint8_t* signature,
                  size_t signature_len,
                  const uint8_t* public_key_info,
                  size_t public_key_info_len);

  // Feeds a piece of the data to the signature verifier.
  void VerifyUpdate(const uint8_t* data_part, size_t data_part_len);

  // Concludes a signature verification operation.  Returns true if the
  // signature is valid.  Returns false if the signature is invalid or an
  // error occurred.
  bool VerifyFinal();

 private:
  void Reset();

  omaha::internal::EcdsaSignature sig_;
  omaha::internal::EcdsaPublicKey key_;
  scoped_ptr<omaha::CryptDetails::HashInterface> hasher_;
};

}  // namespace crypto

#endif  // OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_SIGNATURE_VERIFIER_H_
