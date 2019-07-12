// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/signature_creator.h"

#include <memory>

#include "omaha/base/debug.h"

namespace crypto {

// static
SignatureCreator* SignatureCreator::Create(RSAPrivateKey* key,
                                           ALG_ID algorithm_id) {
  std::unique_ptr<SignatureCreator> result(new SignatureCreator);
  result->key_ = key;

  if (!CryptCreateHash(key->provider(), algorithm_id, 0, 0,
                       result->hash_object_.receive())) {
    ASSERT1(false);
    return NULL;
  }

  return result.release();
}

SignatureCreator::SignatureCreator() : hash_object_(0) {}

SignatureCreator::~SignatureCreator() {}

bool SignatureCreator::Update(const uint8* data_part, int data_part_len) {
  if (!CryptHashData(hash_object_, data_part, data_part_len, 0)) {
    ASSERT1(false);
    return false;
  }

  return true;
}

bool SignatureCreator::Final(std::vector<uint8>* signature) {
  DWORD signature_length = 0;
  if (!CryptSignHash(hash_object_, AT_SIGNATURE, NULL, 0, NULL,
                     &signature_length)) {
    return false;
  }

  std::vector<uint8> temp;
  temp.resize(signature_length);
  if (!CryptSignHash(hash_object_, AT_SIGNATURE, NULL, 0, &temp.front(),
                     &signature_length)) {
    return false;
  }

  temp.resize(signature_length);
  for (size_t i = temp.size(); i > 0; --i)
    signature->push_back(temp[i - 1]);

  return true;
}

}  // namespace crypto

