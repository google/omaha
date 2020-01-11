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
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/signature_verifier.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "omaha/base/debug.h"
#include "omaha/base/security/p256_ecdsa.h"
#include "omaha/base/signatures.h"

namespace crypto {

SignatureVerifier::SignatureVerifier() :
    hasher_(omaha::CryptDetails::CreateHasher()) {}

SignatureVerifier::~SignatureVerifier() {}

bool SignatureVerifier::VerifyInit(SignatureAlgorithm signature_algorithm,
                                   const uint8_t* signature,
                                   size_t signature_len,
                                   const uint8_t* public_key_info,
                                   size_t public_key_info_len) {
  if (signature_algorithm != ECDSA_SHA256) {
    return false;
  }

  std::vector<uint8> signature_vector(signature, signature + signature_len);
  if (!sig_.DecodeFromBuffer(signature_vector)) {
    return false;
  }

  std::vector<uint8> spki(public_key_info,
                          public_key_info + public_key_info_len);
  if (!key_.DecodeSubjectPublicKeyInfo(spki)) {
    return false;
  }

  return true;
}

void SignatureVerifier::VerifyUpdate(const uint8_t* data_part,
                                     size_t data_part_len) {
  ASSERT1(hasher_.get());
  hasher_->update(data_part, data_part_len);
}

bool SignatureVerifier::VerifyFinal() {
  ASSERT1(hasher_.get());
  const uint8_t* digest(hasher_->final());
  p256_int digest_as_int = {};
  p256_from_bin(digest, &digest_as_int);

  bool success(p256_ecdsa_verify(key_.gx(), key_.gy(),
                                 &digest_as_int,
                                 sig_.r(), sig_.s()) != 0);

  Reset();
  return success;
}

void SignatureVerifier::Reset() {
  hasher_.reset();
}

}  // namespace crypto
