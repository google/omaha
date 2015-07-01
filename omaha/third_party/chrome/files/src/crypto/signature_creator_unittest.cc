// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/scoped_ptr.h"
#include "crypto/signature_creator.h"
#include "crypto/signature_verifier.h"
#include "omaha/testing/unit_test.h"

TEST(SignatureCreatorTest, BasicTest) {
  // Do a verify round trip.
  scoped_ptr<crypto::RSAPrivateKey> key_original(
      crypto::RSAPrivateKey::Create(1024));
  ASSERT_TRUE(key_original.get());

  std::vector<uint8> key_info;
  key_original->ExportPrivateKey(&key_info);
  scoped_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_info));
  ASSERT_TRUE(key.get());

  scoped_ptr<crypto::SignatureCreator> signer(
      crypto::SignatureCreator::Create(key.get()));
  ASSERT_TRUE(signer.get());

  std::string data("Hello, World!");
  ASSERT_TRUE(signer->Update(reinterpret_cast<const uint8*>(data.c_str()),
                             static_cast<int>(data.size())));

  std::vector<uint8> signature;
  ASSERT_TRUE(signer->Final(&signature));

  std::vector<uint8> public_key_info;
  ASSERT_TRUE(key_original->ExportPublicKey(&public_key_info));

  // This is the algorithm ID for SHA-1 with RSA encryption.
  // TODO(aa): Factor this out into some shared location.
  const uint8 kSHA1WithRSAAlgorithmID[] = {
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00
  };
  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(
      kSHA1WithRSAAlgorithmID, sizeof(kSHA1WithRSAAlgorithmID),
      &signature.front(), static_cast<int>(signature.size()),
      &public_key_info.front(), static_cast<int>(public_key_info.size())));

  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(data.c_str()),
                        static_cast<int>(data.size()));
  ASSERT_TRUE(verifier.VerifyFinal());
}

