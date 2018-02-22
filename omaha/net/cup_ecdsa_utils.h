// Copyright 2013 Google Inc.
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

#ifndef OMAHA_NET_CUP_ECDSA_UTILS_H_
#define OMAHA_NET_CUP_ECDSA_UTILS_H_

#include <string.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/base/security/p256.h"

namespace omaha {

namespace internal {

// The implementation of SHA-256 in base/security expects buffer sizes as int,
// rather than size_t.  These two wrappers add quick checks for int overflow
// when passing vectors (which can be up to MAX_SIZE_T bytes) and make the
// output a vector.

bool SafeSHA256Hash(const void* data, size_t len,
                    std::vector<uint8>* hash_out);

bool SafeSHA256Hash(const std::vector<uint8>& data,
                    std::vector<uint8>* hash_out);

// EcdsaSignature parses a DER-encoded ASN.1 EcdsaSignature and converts it
// to an (R,S) integer pair in our native 256-bit int implementation.
class EcdsaSignature {
 public:
  EcdsaSignature();

  bool DecodeFromBuffer(const std::vector<uint8>& asn1der);

  const p256_int* r() const { return &r_; }
  const p256_int* s() const { return &s_; }

 private:
  friend class EcdsaSignatureTest;

  bool DoDecodeFromBuffer(const std::vector<uint8>& asn1der);

  static const uint8* DecodeDerInt256(const uint8* buffer_begin,
                                      const uint8* buffer_end,
                                      p256_int* int_out);

  p256_int r_;
  p256_int s_;

  DISALLOW_COPY_AND_ASSIGN(EcdsaSignature);
};

// EcdsaSignature parses a server-provided buffer for an ECDSA public key and
// preps it for use.
class EcdsaPublicKey {
 public:
  EcdsaPublicKey();

  void DecodeFromBuffer(const uint8* encoded_pkey_in);

  // Parses a DER-encoded SubjectPublicKeyInfo value holding a P-256 ECDSA key.
  bool DecodeSubjectPublicKeyInfo(const std::vector<uint8>& spki);

  uint8 version() const { return version_; }
  const p256_int* gx() const { return &gx_; }
  const p256_int* gy() const { return &gy_; }

 private:
  uint8 version_;
  p256_int gx_;
  p256_int gy_;

  DISALLOW_COPY_AND_ASSIGN(EcdsaPublicKey);
};

bool VerifyEcdsaSignature(const EcdsaPublicKey& public_key,
                          const std::vector<uint8>& buffer,
                          const EcdsaSignature& signature);

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_NET_CUP_ECDSA_UTILS_H_


