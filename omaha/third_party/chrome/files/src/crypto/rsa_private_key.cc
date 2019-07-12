// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/rsa_private_key.h"

#include <list>
#include <memory>

#include "omaha/base/debug.h"
#include "omaha/base/logging.h"

namespace crypto {

HRESULT CryptAcquireContextWithFallback(DWORD provider_type,
                                        HCRYPTPROV* provider) {
  ASSERT1(provider);

  const TCHAR* kHashCryptoProviders[] = {
      NULL,                   // The default provider.
      MS_ENH_RSA_AES_PROV,    // The named provider for Vista and up.
      MS_ENH_RSA_AES_PROV_XP  // The named provider for XP SP3 and up.
  };

  // Try different providers until one of them succeeds.
  HRESULT hr = S_OK;
  ScopedHCRYPTPROV scoped_provider;
  for (auto hash_crypto_provider : kHashCryptoProviders) {
    if (::CryptAcquireContext(scoped_provider.receive(),
                              NULL,
                              hash_crypto_provider,
                              provider_type,
                              CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
      REPORT_LOG(L6, (_T("[CryptAcquireContext succeeded]")));
      *provider = scoped_provider.release();
      return S_OK;
    }

    hr = HRESULT_FROM_WIN32(::GetLastError());
    REPORT_LOG(LE, (_T("[CryptAcquireContext failed][%#x]"), hr));
  }

  return hr;
}

// static
RSAPrivateKey* RSAPrivateKey::Create(uint16 num_bits) {
  std::unique_ptr<RSAPrivateKey> result(new RSAPrivateKey());
  if (!result->InitProvider())
    return nullptr;

  DWORD flags = CRYPT_EXPORTABLE;

  // The size is encoded as the upper 16 bits of the flags. :: sigh ::.
  flags |= (num_bits << 16);
  if (!CryptGenKey(result->provider_, CALG_RSA_SIGN, flags,
                   result->key_.receive()))
    return nullptr;

  return result.release();
}

// static
RSAPrivateKey* RSAPrivateKey::CreateSensitive(uint16 num_bits) {
  UNREFERENCED_PARAMETER(num_bits);
  ASSERT1(false);
  return nullptr;
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  std::unique_ptr<RSAPrivateKey> result(new RSAPrivateKey());
  if (!result->InitProvider())
    return nullptr;

  PrivateKeyInfoCodec pki(false);  // Little-Endian
  pki.Import(input);

  size_t blob_size = sizeof(PUBLICKEYSTRUC) +
                     sizeof(RSAPUBKEY) +
                     pki.modulus()->size() +
                     pki.prime1()->size() +
                     pki.prime2()->size() +
                     pki.exponent1()->size() +
                     pki.exponent2()->size() +
                     pki.coefficient()->size() +
                     pki.private_exponent()->size();
  std::unique_ptr<BYTE[]> blob(new BYTE[blob_size]);

  uint8* dest = blob.get();
  PUBLICKEYSTRUC* public_key_struc = reinterpret_cast<PUBLICKEYSTRUC*>(dest);
  public_key_struc->bType = PRIVATEKEYBLOB;
  public_key_struc->bVersion = 0x02;
  public_key_struc->reserved = 0;
  public_key_struc->aiKeyAlg = CALG_RSA_SIGN;
  dest += sizeof(PUBLICKEYSTRUC);

  RSAPUBKEY* rsa_pub_key = reinterpret_cast<RSAPUBKEY*>(dest);
  rsa_pub_key->magic = 0x32415352;
  rsa_pub_key->bitlen = static_cast<DWORD>(pki.modulus()->size() * 8);
  int public_exponent_int = 0;
  for (size_t i = pki.public_exponent()->size(); i > 0; --i) {
    public_exponent_int <<= 8;
    public_exponent_int |= (*pki.public_exponent())[i - 1];
  }
  rsa_pub_key->pubexp = public_exponent_int;
  dest += sizeof(RSAPUBKEY);

  memcpy(dest, &pki.modulus()->front(), pki.modulus()->size());
  dest += pki.modulus()->size();
  memcpy(dest, &pki.prime1()->front(), pki.prime1()->size());
  dest += pki.prime1()->size();
  memcpy(dest, &pki.prime2()->front(), pki.prime2()->size());
  dest += pki.prime2()->size();
  memcpy(dest, &pki.exponent1()->front(), pki.exponent1()->size());
  dest += pki.exponent1()->size();
  memcpy(dest, &pki.exponent2()->front(), pki.exponent2()->size());
  dest += pki.exponent2()->size();
  memcpy(dest, &pki.coefficient()->front(), pki.coefficient()->size());
  dest += pki.coefficient()->size();
  memcpy(dest, &pki.private_exponent()->front(),
         pki.private_exponent()->size());
  dest += pki.private_exponent()->size();

  ASSERT1(dest == blob.get() + blob_size);
  if (!CryptImportKey(result->provider_,
                      reinterpret_cast<uint8*>(public_key_struc),
                      static_cast<DWORD>(blob_size), 0,
                      CRYPT_EXPORTABLE, result->key_.receive()))
    return nullptr;

  return result.release();
}

// static
RSAPrivateKey* RSAPrivateKey::CreateSensitiveFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  UNREFERENCED_PARAMETER(input);
  ASSERT1(false);
  return nullptr;
}

// static
RSAPrivateKey* RSAPrivateKey::FindFromPublicKeyInfo(
    const std::vector<uint8>& input) {
  UNREFERENCED_PARAMETER(input);
  ASSERT1(false);
  return nullptr;
}

RSAPrivateKey::RSAPrivateKey() : provider_(NULL), key_(NULL) {}

RSAPrivateKey::~RSAPrivateKey() {}

bool RSAPrivateKey::InitProvider() {
  return SUCCEEDED(CryptAcquireContextWithFallback(PROV_RSA_AES,
                                                   provider_.receive()));
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8>* output) {
  // Export the key
  DWORD blob_length = 0;
  if (!CryptExportKey(key_, 0, PRIVATEKEYBLOB, 0, NULL, &blob_length)) {
    ASSERT1(false);
    return false;
  }

  std::unique_ptr<uint8[]> blob(new uint8[blob_length]);
  if (!CryptExportKey(key_, 0, PRIVATEKEYBLOB, 0, blob.get(), &blob_length)) {
    ASSERT1(false);
    return false;
  }

  uint8* pos = blob.get();
  PUBLICKEYSTRUC *publickey_struct = reinterpret_cast<PUBLICKEYSTRUC*>(pos);
  pos += sizeof(PUBLICKEYSTRUC);

  RSAPUBKEY *rsa_pub_key = reinterpret_cast<RSAPUBKEY*>(pos);
  pos += sizeof(RSAPUBKEY);

  int mod_size = rsa_pub_key->bitlen / 8;
  int primes_size = rsa_pub_key->bitlen / 16;

  PrivateKeyInfoCodec pki(false);  // Little-Endian

  pki.modulus()->assign(pos, pos + mod_size);
  pos += mod_size;

  pki.prime1()->assign(pos, pos + primes_size);
  pos += primes_size;
  pki.prime2()->assign(pos, pos + primes_size);
  pos += primes_size;

  pki.exponent1()->assign(pos, pos + primes_size);
  pos += primes_size;
  pki.exponent2()->assign(pos, pos + primes_size);
  pos += primes_size;

  pki.coefficient()->assign(pos, pos + primes_size);
  pos += primes_size;

  pki.private_exponent()->assign(pos, pos + mod_size);
  pos += mod_size;

  pki.public_exponent()->assign(reinterpret_cast<uint8*>(&rsa_pub_key->pubexp),
      reinterpret_cast<uint8*>(&rsa_pub_key->pubexp) + 4);

  ASSERT1(pos - blob_length == reinterpret_cast<BYTE*>(publickey_struct));

  return pki.Export(output);
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  DWORD key_info_len;
  if (!CryptExportPublicKeyInfo(
      provider_, AT_SIGNATURE, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      NULL, &key_info_len)) {
    ASSERT1(false);
    return false;
  }

  std::unique_ptr<uint8[]> key_info(new uint8[key_info_len]);
  if (!CryptExportPublicKeyInfo(
      provider_, AT_SIGNATURE, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      reinterpret_cast<CERT_PUBLIC_KEY_INFO*>(key_info.get()), &key_info_len)) {
    ASSERT1(false);
    return false;
  }

  DWORD encoded_length;
  if (!CryptEncodeObject(
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
      reinterpret_cast<CERT_PUBLIC_KEY_INFO*>(key_info.get()), NULL,
      &encoded_length)) {
    ASSERT1(false);
    return false;
  }

  std::unique_ptr<BYTE[]> encoded(new BYTE[encoded_length]);
  if (!CryptEncodeObject(
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
      reinterpret_cast<CERT_PUBLIC_KEY_INFO*>(key_info.get()), encoded.get(),
      &encoded_length)) {
    ASSERT1(false);
    return false;
  }

  for (size_t i = 0; i < encoded_length; ++i)
    output->push_back(encoded[i]);

  return true;
}

}  // namespace crypto

