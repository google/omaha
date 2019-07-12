// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_RSA_PRIVATE_KEY_H_
#define OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_RSA_PRIVATE_KEY_H_
#pragma once

#include "build/build_config.h"

#if defined(USE_OPENSSL)
// Forward declaration for openssl/*.h
typedef struct evp_pkey_st EVP_PKEY;
#elif defined(USE_NSS)
// Forward declaration.
struct SECKEYPrivateKeyStr;
struct SECKEYPublicKeyStr;
#elif defined(OS_MACOSX)
#include <Security/cssm.h>
#endif

#include <list>
#include <vector>

#include "base/basictypes.h"

#if defined(OS_WIN)
#include "crypto/scoped_capi_types.h"
#endif
#if defined(USE_NSS)
#include "base/gtest_prod_util.h"
#endif

namespace crypto {

// Providers implementing SHA256 can be instantiated using different names.
// On Vista and up, both the default provider and the enhanced RSA/AES
// provider support SHA256. On Windows XP, the named provider has a different
// name, therefore, the code falls back to a specific named provider in case
// of errors.
HRESULT CryptAcquireContextWithFallback(DWORD provider_type,
                                        HCRYPTPROV* provider);

// Used internally by RSAPrivateKey for serializing and deserializing
// PKCS #8 PrivateKeyInfo and PublicKeyInfo.
class PrivateKeyInfoCodec {
 public:
  // ASN.1 encoding of the AlgorithmIdentifier from PKCS #8.
  static const uint8 kRsaAlgorithmIdentifier[];

  // ASN.1 tags for some types we use.
  static const uint8 kBitStringTag = 0x03;
  static const uint8 kIntegerTag = 0x02;
  static const uint8 kNullTag = 0x05;
  static const uint8 kOctetStringTag = 0x04;
  static const uint8 kSequenceTag = 0x30;

  // |big_endian| here specifies the byte-significance of the integer components
  // that will be parsed & serialized (modulus(), etc...) during Import(),
  // Export() and ExportPublicKeyInfo() -- not the ASN.1 DER encoding of the
  // PrivateKeyInfo/PublicKeyInfo (which is always big-endian).
  explicit PrivateKeyInfoCodec(bool big_endian);

  ~PrivateKeyInfoCodec();

  // Exports the contents of the integer components to the ASN.1 DER encoding
  // of the PrivateKeyInfo structure to |output|.
  bool Export(std::vector<uint8>* output);

  // Exports the contents of the integer components to the ASN.1 DER encoding
  // of the PublicKeyInfo structure to |output|.
  bool ExportPublicKeyInfo(std::vector<uint8>* output);

  // Exports the contents of the integer components to the ASN.1 DER encoding
  // of the RSAPublicKey structure to |output|.
  bool ExportPublicKey(std::vector<uint8>* output);

  // Parses the ASN.1 DER encoding of the PrivateKeyInfo structure in |input|
  // and populates the integer components with |big_endian_| byte-significance.
  // IMPORTANT NOTE: This is currently *not* security-approved for importing
  // keys from unstrusted sources.
  bool Import(const std::vector<uint8>& input);

  // Accessors to the contents of the integer components of the PrivateKeyInfo
  // structure.
  std::vector<uint8>* modulus() { return &modulus_; }
  std::vector<uint8>* public_exponent() { return &public_exponent_; }
  std::vector<uint8>* private_exponent() { return &private_exponent_; }
  std::vector<uint8>* prime1() { return &prime1_; }
  std::vector<uint8>* prime2() { return &prime2_; }
  std::vector<uint8>* exponent1() { return &exponent1_; }
  std::vector<uint8>* exponent2() { return &exponent2_; }
  std::vector<uint8>* coefficient() { return &coefficient_; }

 private:
  // Utility wrappers for PrependIntegerImpl that use the class's |big_endian_|
  // value.
  void PrependInteger(const std::vector<uint8>& in, std::list<uint8>* out);
  void PrependInteger(uint8* val, size_t num_bytes, std::list<uint8>* data);

  // Prepends the integer stored in |val| - |val + num_bytes| with |big_endian|
  // byte-significance into |data| as an ASN.1 integer.
  void PrependIntegerImpl(uint8* val,
                          size_t num_bytes,
                          std::list<uint8>* data,
                          bool big_endian);

  // Utility wrappers for ReadIntegerImpl that use the class's |big_endian_|
  // value.
  bool ReadInteger(uint8** pos, uint8* end, std::vector<uint8>* out);
  bool ReadIntegerWithExpectedSize(uint8** pos,
                                   uint8* end,
                                   size_t expected_size,
                                   std::vector<uint8>* out);

  // Reads an ASN.1 integer from |pos|, and stores the result into |out| with
  // |big_endian| byte-significance.
  bool ReadIntegerImpl(uint8** pos,
                       uint8* end,
                       std::vector<uint8>* out,
                       bool big_endian);

  // Prepends the integer stored in |val|, starting a index |start|, for
  // |num_bytes| bytes onto |data|.
  void PrependBytes(uint8* val,
                    size_t start,
                    size_t num_bytes,
                    std::list<uint8>* data);

  // Helper to prepend an ASN.1 length field.
  void PrependLength(size_t size, std::list<uint8>* data);

  // Helper to prepend an ASN.1 type header.
  void PrependTypeHeaderAndLength(uint8 type,
                                  size_t length,
                                  std::list<uint8>* output);

  // Helper to prepend an ASN.1 bit string
  void PrependBitString(uint8* val, int num_bytes, std::list<uint8>* output);

  // Read an ASN.1 length field. This also checks that the length does not
  // extend beyond |end|.
  bool ReadLength(uint8** pos, uint8* end, uint32* result);

  // Read an ASN.1 type header and its length.
  bool ReadTypeHeaderAndLength(uint8** pos,
                               uint8* end,
                               uint8 expected_tag,
                               uint32* length);

  // Read an ASN.1 sequence declaration. This consumes the type header and
  // length field, but not the contents of the sequence.
  bool ReadSequence(uint8** pos, uint8* end);

  // Read the RSA AlgorithmIdentifier.
  bool ReadAlgorithmIdentifier(uint8** pos, uint8* end);

  // Read one of the two version fields in PrivateKeyInfo.
  bool ReadVersion(uint8** pos, uint8* end);

  // The byte-significance of the stored components (modulus, etc..).
  bool big_endian_;

  // Component integers of the PrivateKeyInfo
  std::vector<uint8> modulus_;
  std::vector<uint8> public_exponent_;
  std::vector<uint8> private_exponent_;
  std::vector<uint8> prime1_;
  std::vector<uint8> prime2_;
  std::vector<uint8> exponent1_;
  std::vector<uint8> exponent2_;
  std::vector<uint8> coefficient_;

  DISALLOW_COPY_AND_ASSIGN(PrivateKeyInfoCodec);
};

// Encapsulates an RSA private key. Can be used to generate new keys, export
// keys to other formats, or to extract a public key.
// TODO(hclam): This class should be ref-counted so it can be reused easily.
class RSAPrivateKey {
 public:
  ~RSAPrivateKey();

  // Create a new random instance. Can return NULL if initialization fails.
  static RSAPrivateKey* Create(uint16 num_bits);

  // Create a new random instance. Can return NULL if initialization fails.
  // The created key is permanent and is not exportable in plaintext form.
  //
  // NOTE: Currently only available if USE_NSS is defined.
  static RSAPrivateKey* CreateSensitive(uint16 num_bits);

  // Create a new instance by importing an existing private key. The format is
  // an ASN.1-encoded PrivateKeyInfo block from PKCS #8. This can return NULL if
  // initialization fails.
  static RSAPrivateKey* CreateFromPrivateKeyInfo(
      const std::vector<uint8>& input);

  // Create a new instance by importing an existing private key. The format is
  // an ASN.1-encoded PrivateKeyInfo block from PKCS #8. This can return NULL if
  // initialization fails.
  // The created key is permanent and is not exportable in plaintext form.
  //
  // NOTE: Currently only available if USE_NSS is defined.
  static RSAPrivateKey* CreateSensitiveFromPrivateKeyInfo(
      const std::vector<uint8>& input);

  // Import an existing public key, and then search for the private
  // half in the key database. The format of the public key blob is is
  // an X509 SubjectPublicKeyInfo block. This can return NULL if
  // initialization fails or the private key cannot be found.  The
  // caller takes ownership of the returned object, but nothing new is
  // created in the key database.
  //
  // NOTE: Currently only available if USE_NSS is defined.
  static RSAPrivateKey* FindFromPublicKeyInfo(
      const std::vector<uint8>& input);

#if defined(USE_OPENSSL)
  EVP_PKEY* key() { return key_; }
#elif defined(USE_NSS)
  SECKEYPrivateKeyStr* key() { return key_; }
  SECKEYPublicKeyStr* public_key() { return public_key_; }
#elif defined(OS_WIN)
  HCRYPTPROV provider() { return provider_; }
  HCRYPTKEY key() { return key_; }
#elif defined(OS_MACOSX)
  CSSM_KEY_PTR key() { return &key_; }
  CSSM_KEY_PTR public_key() { return &public_key_; }
#endif

  // Exports the private key to a PKCS #1 PrivateKey block.
  bool ExportPrivateKey(std::vector<uint8>* output);

  // Exports the public key to an X509 SubjectPublicKeyInfo block.
  bool ExportPublicKey(std::vector<uint8>* output);

 private:
#if defined(USE_NSS)
  FRIEND_TEST_ALL_PREFIXES(RSAPrivateKeyNSSTest, FindFromPublicKey);
  FRIEND_TEST_ALL_PREFIXES(RSAPrivateKeyNSSTest, FailedFindFromPublicKey);
#endif

  // Constructor is private. Use one of the Create*() or Find*()
  // methods above instead.
  RSAPrivateKey();

  // Shared helper for Create() and CreateSensitive().
  // TODO(cmasone): consider replacing |permanent| and |sensitive| with a
  //                flags arg created by ORing together some enumerated values.
  static RSAPrivateKey* CreateWithParams(uint16 num_bits,
                                         bool permanent,
                                         bool sensitive);

  // Shared helper for CreateFromPrivateKeyInfo() and
  // CreateSensitiveFromPrivateKeyInfo().
  static RSAPrivateKey* CreateFromPrivateKeyInfoWithParams(
      const std::vector<uint8>& input, bool permanent, bool sensitive);

#if defined(USE_OPENSSL)
  EVP_PKEY* key_;
#elif defined(USE_NSS)
  SECKEYPrivateKeyStr* key_;
  SECKEYPublicKeyStr* public_key_;
#elif defined(OS_WIN)
  bool InitProvider();

  ScopedHCRYPTPROV provider_;
  ScopedHCRYPTKEY key_;
#elif defined(OS_MACOSX)
  CSSM_KEY key_;
  CSSM_KEY public_key_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RSAPrivateKey);
};

}  // namespace crypto

#endif  // OMAHA_THIRD_PARTY_CHROME_FILES_SRC_CRYPTO_RSA_PRIVATE_KEY_H_

