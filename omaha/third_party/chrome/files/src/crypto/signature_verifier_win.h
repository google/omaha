// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_SIGNATURE_VERIFIER_WIN_H_
#define OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_SIGNATURE_VERIFIER_WIN_H_

#include <array>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "crypto/scoped_capi_types.h"

// defines from wincrypt.h. These are copied here because we currently compile
// with support for XP, and these defines are valid for XP SP2 and above.
#define ALG_SID_SHA_256  12
#define CALG_SHA_256     (ALG_CLASS_HASH | ALG_TYPE_ANY | ALG_SID_SHA_256)

namespace crypto {

// The SignatureVerifierWin class verifies a signature using a bare public key
// (as opposed to a certificate).
class SignatureVerifierWin {
 public:
  SignatureVerifierWin();
  ~SignatureVerifierWin();

  // Streaming interface:

  // Initiates a signature verification operation.  This should be followed
  // by one or more VerifyUpdate calls and a VerifyFinal call.
  //
  // The |signature| is encoded according to the |algorithm_id|, but it must not
  // be further encoded in an ASN.1 BIT STRING.
  // Note: An RSA signatures is actually a big integer.  It must be in the
  // big-endian byte order.
  //
  // The public key is specified as a DER encoded ASN.1 SubjectPublicKeyInfo
  // structure, which contains not only the public key but also its type
  // (algorithm):
  //   SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //       algorithm            AlgorithmIdentifier,
  //       subjectPublicKey     BIT STRING  }
  bool VerifyInit(ALG_ID algorithm_id,
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

  std::vector<uint8_t> signature_;
  ScopedHCRYPTPROV provider_;
  ScopedHCRYPTHASH hash_object_;
  ScopedHCRYPTKEY public_key_;
};

}  // namespace crypto

#endif  // OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_SIGNATURE_VERIFIER_WIN_H_
