// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/signature_creator.h"

#include <memory>
#include <vector>

#include "crypto/signature_verifier_win.h"
#include "omaha/testing/unit_test.h"

TEST(SignatureCreatorTest, BasicTest) {
  // Do a verify round trip.
  std::unique_ptr<crypto::RSAPrivateKey> key_original(
      crypto::RSAPrivateKey::Create(1024));
  ASSERT_TRUE(key_original.get());

  std::vector<uint8_t> key_info;
  key_original->ExportPrivateKey(&key_info);
  std::unique_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_info));
  ASSERT_TRUE(key.get());

  std::unique_ptr<crypto::SignatureCreator> signer(
      crypto::SignatureCreator::Create(key.get(), CALG_SHA1));
  ASSERT_TRUE(signer.get());

  std::string data("Hello, World!");
  ASSERT_TRUE(signer->Update(reinterpret_cast<const uint8_t*>(data.c_str()),
                             static_cast<int>(data.size())));

  std::vector<uint8_t> signature;
  ASSERT_TRUE(signer->Final(&signature));

  std::vector<uint8_t> public_key_info;
  ASSERT_TRUE(key_original->ExportPublicKey(&public_key_info));

  crypto::SignatureVerifierWin verifier;
  ASSERT_TRUE(verifier.VerifyInit(
      CALG_SHA1,
      &signature.front(), static_cast<int>(signature.size()),
      &public_key_info.front(), static_cast<int>(public_key_info.size())));

  verifier.VerifyUpdate(reinterpret_cast<const uint8_t*>(data.c_str()),
                        static_cast<int>(data.size()));
  ASSERT_TRUE(verifier.VerifyFinal());
}

