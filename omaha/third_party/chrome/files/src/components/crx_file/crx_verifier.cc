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
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/crx_verifier.h"

#pragma warning(disable : 4245)
// C4245 : conversion from 'type1' to 'type2', signed/unsigned mismatch
#include <atlenc.h>
#pragma warning(default : 4245)

#include <cstring>
#include <iterator>
#include <memory>
#include <set>
#include <utility>

#include "components/crx_file/id_util.h"
#include "crypto/secure_util.h"
#include "crypto/signature_verifier.h"
#include "omaha/base/debug.h"
#include "omaha/base/file.h"
#include "omaha/base/security/sha256.h"
#include "omaha/base/signatures.h"
#include "omaha/net/cup_ecdsa_utils.h"
#include "third_party/bar/shared_ptr.h"
#include "third_party/chrome/files/src/components/crx_file/crx3.pb.h"

namespace crx_file {

namespace {

// The maximum size the Crx3 parser will tolerate for a header.
const uint32_t kMaxHeaderSize = 1 << 18;

// The context for Crx3 signing, encoded in UTF8.
const unsigned char kSignatureContext[] = u8"CRX3 SignedData";

// The SHA256 hash of the "ecdsa_2017_public" Crx3 key.
const uint8_t kPublisherKeyHash[] = {
    0x61, 0xf7, 0xf2, 0xa6, 0xbf, 0xcf, 0x74, 0xcd, 0x0b, 0xc1, 0xfe,
    0x24, 0x97, 0xcc, 0x9b, 0x04, 0x25, 0x4c, 0x65, 0x8f, 0x79, 0xf2,
    0x14, 0x53, 0x92, 0x86, 0x7e, 0xa8, 0x36, 0x63, 0x67, 0xcf};

typedef std::vector<shared_ptr<crypto::SignatureVerifier>> Verifiers;
typedef google::protobuf::RepeatedPtrField<AsymmetricKeyProof> RepeatedProof;

// Returns the number of bytes read, or -1 in the case of an unexpected EOF or
// read error.
int ReadAndHashBuffer(uint8_t* buffer,
                      int length,
                      omaha::File* file,
                      omaha::CryptDetails::HashInterface* hash) {
  ASSERT1(sizeof(byte) == sizeof(uint8_t));
  uint32 bytes_read = 0;
  HRESULT hr = file->Read(length, reinterpret_cast<byte*>(buffer), &bytes_read);
  if (FAILED(hr)) {
    return -1;
  }

  hash->update(buffer, bytes_read);
  return bytes_read;
}

// Returns UINT32_MAX in the case of an unexpected EOF or read error, else
// returns the read uint32.
uint32_t ReadAndHashLittleEndianUInt32(
    omaha::File* file,
    omaha::CryptDetails::HashInterface* hash) {
  uint8_t buffer[4] = {};
  if (ReadAndHashBuffer(buffer, 4, file, hash) != 4) {
    return UINT32_MAX;
  }
  return buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0];
}

// Read to the end of the file, updating the hash and all verifiers.
bool ReadHashAndVerifyArchive(omaha::File* file,
                              omaha::CryptDetails::HashInterface* hash,
                              const Verifiers& verifiers) {
  uint8_t buffer[1 << 12] = {};
  size_t len = 0;
  while ((len = ReadAndHashBuffer(buffer, arraysize(buffer), file, hash)) > 0) {
    for (Verifiers::const_iterator verifier = verifiers.begin();
         verifier != verifiers.end();
         ++verifier) {
      (*verifier)->VerifyUpdate(buffer, len);
    }
  }

  if (len < 0) {
    return false;
  }

  for (Verifiers::const_iterator verifier = verifiers.begin();
       verifier != verifiers.end();
       ++verifier) {
    if (!(*verifier)->VerifyFinal()) {
      return false;
    }
  }
  return true;
}

// The remaining contents of a Crx3 file are [header-size][header][archive].
// [header] is an encoded protocol buffer and contains both a signed and
// unsigned section. The unsigned section contains a set of key/signature pairs,
// and the signed section is the encoding of another protocol buffer. All
// signatures cover [prefix][signed-header-size][signed-header][archive].
VerifierResult VerifyCrx3(
    omaha::File* file,
    omaha::CryptDetails::HashInterface* hash,
    const std::vector<std::vector<uint8_t>>& required_key_hashes,
    std::string* public_key,
    std::string* crx_id,
    bool require_publisher_key) {
  // Parse [header-size] and [header].
  const uint32_t header_size = ReadAndHashLittleEndianUInt32(file, hash);
  if (header_size > kMaxHeaderSize) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }
  std::vector<uint8_t> header_bytes(header_size);
  // Assuming kMaxHeaderSize can fit in an int, the following cast is safe.
  if (ReadAndHashBuffer(header_bytes.data(), header_size, file, hash) !=
      static_cast<int>(header_size)) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }
  CrxFileHeader header;
  if (!header.ParseFromArray(header_bytes.data(), header_size)) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }

  // Parse [signed-header].
  const std::string& signed_header_data_str = header.signed_header_data();
  SignedData signed_header_data;
  if (!signed_header_data.ParseFromString(signed_header_data_str)) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }
  const std::string& crx_id_encoded = signed_header_data.crx_id();
  const std::string declared_crx_id = id_util::GenerateIdFromHex(
      id_util::HexEncode(crx_id_encoded.data(), crx_id_encoded.size()));

  // Create a little-endian representation of [signed-header-size].
  const int signed_header_size = signed_header_data_str.size();
  const uint8_t header_size_octets[] = {
    static_cast<const uint8_t>(signed_header_size),
    static_cast<const uint8_t>(signed_header_size >> 8),
    static_cast<const uint8_t>(signed_header_size >> 16),
    static_cast<const uint8_t>(signed_header_size >> 24),
  };

  // Create a set of all required key hashes.
  std::set<std::vector<uint8_t>> required_key_set(required_key_hashes.begin(),
                                                  required_key_hashes.end());
  if (require_publisher_key) {
    required_key_set.emplace(std::begin(kPublisherKeyHash),
                             std::end(kPublisherKeyHash));
  }

  typedef const RepeatedProof& (CrxFileHeader::*ProofFetcher)() const;
  ProofFetcher rsa = &CrxFileHeader::sha256_with_rsa;
  ProofFetcher ecdsa = &CrxFileHeader::sha256_with_ecdsa;

  std::string public_key_bytes;
  Verifiers verifiers;
  verifiers.reserve(header.sha256_with_ecdsa_size());

  typedef std::pair<ProofFetcher,
                    crypto::SignatureVerifier::SignatureAlgorithm> ProofType;
  typedef std::vector<ProofType> ProofTypes;

  const ProofTypes proof_types = {
    std::make_pair(rsa, crypto::SignatureVerifier::RSA_PKCS1_SHA256),
    std::make_pair(ecdsa, crypto::SignatureVerifier::ECDSA_SHA256)
  };

  // Initialize all verifiers and update them with
  // [prefix][signed-header-size][signed-header].
  // Clear any elements of required_key_set that are encountered, and watch for
  // the developer key.
  for (ProofTypes::const_iterator proof_type = proof_types.begin();
       proof_type != proof_types.end();
       ++proof_type) {
    const RepeatedProof& repeated_proof = (header.*proof_type->first)();
    for (RepeatedProof::const_iterator& proof = repeated_proof.begin();
         proof != repeated_proof.end();
         ++proof) {
      const std::string& key = proof->public_key();
      const std::string& sig = proof->signature();
      if (id_util::GenerateId(key) == declared_crx_id) {
        public_key_bytes = key;
      }

      std::vector<uint8_t> key_hash(SHA256_DIGEST_SIZE);
      VERIFY1(omaha::internal::SafeSHA256Hash(key.data(),
                                              key.size(),
                                              &key_hash));
      required_key_set.erase(key_hash);

      if (proof_type->second == crypto::SignatureVerifier::RSA_PKCS1_SHA256) {
        // Skipping RSA verification. Verifying ECDSA is sufficient.
        continue;
      }

      shared_ptr<crypto::SignatureVerifier> v(new crypto::SignatureVerifier);
      ASSERT1(sizeof(unsigned char) == sizeof(uint8_t));

      if (!v->VerifyInit(
              proof_type->second, reinterpret_cast<const uint8_t*>(sig.data()),
              sig.size(), reinterpret_cast<const uint8_t*>(key.data()),
              key.size())) {
        return VerifierResult::ERROR_SIGNATURE_INITIALIZATION_FAILED;
      }
      v->VerifyUpdate(kSignatureContext, arraysize(kSignatureContext));
      v->VerifyUpdate(header_size_octets, arraysize(header_size_octets));
      v->VerifyUpdate(
          reinterpret_cast<const uint8_t*>(signed_header_data_str.data()),
          signed_header_data_str.size());
      verifiers.push_back(v);
    }
  }
  if (public_key_bytes.empty() || !required_key_set.empty()) {
    return VerifierResult::ERROR_REQUIRED_PROOF_MISSING;
  }

  // Update and finalize the verifiers with [archive].
  if (!ReadHashAndVerifyArchive(file, hash, verifiers)) {
    return VerifierResult::ERROR_SIGNATURE_VERIFICATION_FAILED;
  }

  const std::vector<byte> v(
      public_key_bytes.c_str(),
      public_key_bytes.c_str() + public_key_bytes.length());
  CStringA encoded;
  VERIFY1(SUCCEEDED(omaha::Base64::Encode(v, &encoded, false)));
  *public_key = encoded;
  *crx_id = declared_crx_id;
  return VerifierResult::OK_FULL;
}

}  // namespace

VerifierResult Verify(
    const std::string& crx_path,
    const VerifierFormat& format,
    const std::vector<std::vector<uint8_t>>& required_key_hashes,
    const std::vector<uint8_t>& required_file_hash,
    std::string* public_key,
    std::string* crx_id) {
  std::string public_key_local;
  std::string crx_id_local;
  if (!omaha::File::Exists(CString(crx_path.c_str()))) {
    return VerifierResult::ERROR_FILE_NOT_READABLE;
  }

  omaha::File file;
  HRESULT hr = file.Open(CString(crx_path.c_str()), false, false);
  if (FAILED(hr)) {
    return VerifierResult::ERROR_FILE_NOT_READABLE;
  }

  shared_ptr<omaha::CryptDetails::HashInterface> file_hash(
      omaha::CryptDetails::CreateHasher(true));

  // Magic number.
  bool diff = false;
  char buffer[kCrx2FileHeaderMagicSize] = {};
  uint32 bytes_read = 0;
  if (FAILED(file.Read(kCrx2FileHeaderMagicSize,
                       reinterpret_cast<byte *>(buffer),
                       &bytes_read)) ||
      bytes_read != kCrx2FileHeaderMagicSize) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }

  if (!strncmp(buffer, kCrxDiffFileHeaderMagic, kCrx2FileHeaderMagicSize)) {
    diff = true;
  } else if (strncmp(buffer, kCrx2FileHeaderMagic, kCrx2FileHeaderMagicSize)) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }
  file_hash->update(buffer, sizeof(buffer));

  // Version number.
  const uint32_t version =
      ReadAndHashLittleEndianUInt32(&file, file_hash.get());
  VerifierResult result =
    version == 3 ?
    VerifyCrx3(&file,
               file_hash.get(),
               required_key_hashes,
               &public_key_local,
               &crx_id_local,
               format == VerifierFormat::CRX3_WITH_PUBLISHER_PROOF) :
    VerifierResult::ERROR_HEADER_INVALID;
  if (result != VerifierResult::OK_FULL) {
    return result;
  }

  if (!required_file_hash.empty()) {
    if (required_file_hash.size() != SHA256_DIGEST_SIZE) {
      return VerifierResult::ERROR_EXPECTED_HASH_INVALID;
    }
    if (!crypto::SecureMemEqual(file_hash->final(),
                                required_file_hash.data(),
                                SHA256_DIGEST_SIZE)) {
      return VerifierResult::ERROR_FILE_HASH_FAILED;
    }
  }

  // All is well. Set the out-params and return.
  if (public_key) {
    *public_key = public_key_local;
  }
  if (crx_id) {
    *crx_id = crx_id_local;
  }
  return diff ? VerifierResult::OK_DELTA : VerifierResult::OK_FULL;
}

}  // namespace crx_file

