// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_SIGNATURE_CREATOR_H_
#define OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_SIGNATURE_CREATOR_H_

#include <vector>

#include "base/basictypes.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_capi_types.h"

namespace crypto {

// Signs data using a bare private key (as opposed to a full certificate).
// Currently can only sign data using SHA-1 with RSA encryption.
class SignatureCreator {
 public:
  ~SignatureCreator();

  // Create an instance. The caller must ensure that the provided PrivateKey
  // instance outlives the created SignatureCreator.
  static SignatureCreator* Create(RSAPrivateKey* key, ALG_ID algorithm_id);

  // Update the signature with more data.
  bool Update(const uint8* data_part, int data_part_len);

  // Finalize the signature.
  bool Final(std::vector<uint8>* signature);

 private:
  // Private constructor. Use the Create() method instead.
  SignatureCreator();

  RSAPrivateKey* key_;

  ScopedHCRYPTHASH hash_object_;

  DISALLOW_COPY_AND_ASSIGN(SignatureCreator);
};

}  // namespace crypto

#endif  // OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_SIGNATURE_CREATOR_H_

