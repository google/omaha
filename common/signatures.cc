// Copyright 2003-2009 Google Inc.
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
// signatures.cpp
//
// Classes and functions related to crypto-hashes of buffers and digital
// signatures of buffers.

#include "omaha/common/signatures.h"
#include <wincrypt.h>
#include <memory.h>

#pragma warning(disable : 4245)
// C4245 : conversion from 'type1' to 'type2', signed/unsigned mismatch
#include <atlenc.h>
#pragma warning(default : 4245)
#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/common/const_utils.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/sha.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"

namespace omaha {

const ALG_ID kHashAlgorithm = CALG_SHA1;
const DWORD kEncodingType = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
const DWORD kProviderType = PROV_RSA_FULL;
const DWORD kCertificateNameType = CERT_NAME_SIMPLE_DISPLAY_TYPE;
const DWORD kKeyPairType = AT_SIGNATURE;

namespace CryptDetails {

  // Useful scoped pointers for working with CryptoAPI objects

  void crypt_release_context(HCRYPTPROV provider) {
    UTIL_LOG(L3, (L"Releasing HCRYPTPROV 0x%08lx", provider));
    BOOL b = ::CryptReleaseContext(provider, 0 /*flags*/);
    ASSERT(b, (L""));
  }

  void crypt_close_store(HCERTSTORE store) {
    UTIL_LOG(L3, (L"Releasing HCERTSTORE 0x%08lx", store));
    BOOL b = ::CertCloseStore(store, 0 /*flags*/);
    ASSERT(b, (L""));
    ASSERT(::GetLastError() != CRYPT_E_PENDING_CLOSE, (L""));
  }

  void crypt_free_certificate(PCCERT_CONTEXT certificate) {
    UTIL_LOG(L3, (L"Releasing PCCERT_CONTEXT 0x%08lx", certificate));
    BOOL b = ::CertFreeCertificateContext(certificate);
    ASSERT(b, (L""));
  }

  void crypt_destroy_key(HCRYPTKEY key) {
    UTIL_LOG(L3, (L"Releasing HCRYPTKEY 0x%08lx", key));
    BOOL b = ::CryptDestroyKey(key);
    ASSERT(b, (L""));
  }

  void crypt_destroy_hash(HCRYPTHASH hash) {
    UTIL_LOG(L3, (L"Releasing HCRYPTHASH 0x%08lx", hash));
    BOOL b = ::CryptDestroyHash(hash);
    ASSERT(b, (L""));
  }

  typedef close_fun<void (*)(HCRYPTHASH),
                    crypt_destroy_hash> smart_destroy_hash;
  typedef scoped_any<HCRYPTHASH, smart_destroy_hash, null_t> scoped_crypt_hash;
}

// Base64 encode/decode functions are part of ATL Server
HRESULT Base64::Encode(const std::vector<byte>& buffer_in,
                       std::vector<byte>* encoded,
                       bool break_into_lines) {
  ASSERT(encoded, (L""));

  if (buffer_in.empty()) {
    encoded->resize(0);
    return S_OK;
  }

  int32 encoded_len =
    Base64EncodeGetRequiredLength(
        buffer_in.size(),
        break_into_lines ? ATL_BASE64_FLAG_NONE : ATL_BASE64_FLAG_NOCRLF);
  ASSERT(encoded_len > 0, (L""));

  encoded->resize(encoded_len);
  int32 str_out_len = encoded_len;

  BOOL result = Base64Encode(
      &buffer_in.front(),
      buffer_in.size(),
      reinterpret_cast<char*>(&encoded->front()),
      &str_out_len,
      break_into_lines ? ATL_BASE64_FLAG_NONE : ATL_BASE64_FLAG_NOCRLF);
  if (!result)
    return E_FAIL;
  ASSERT(str_out_len <= encoded_len, (L""));
  if (str_out_len < encoded_len)
    encoded->resize(str_out_len);

  return S_OK;
}

HRESULT Base64::Encode(const std::vector<byte>& buffer_in,
                       CStringA* encoded,
                       bool break_into_lines) {
  ASSERT(encoded, (L""));

  if (buffer_in.empty()) {
    return S_OK;
  }

  std::vector<byte> buffer_out;
  RET_IF_FAILED(Encode(buffer_in, &buffer_out, break_into_lines));
  encoded->Append(reinterpret_cast<const char*>(&buffer_out.front()),
                                                buffer_out.size());

  return S_OK;
}

HRESULT Base64::Encode(const std::vector<byte>& buffer_in,
                       CString* encoded,
                       bool break_into_lines) {
  ASSERT(encoded, (L""));

  CStringA string_out;
  RET_IF_FAILED(Encode(buffer_in, &string_out, break_into_lines));
  *encoded = string_out;

  return S_OK;
}

HRESULT Base64::Decode(const std::vector<byte>& encoded,
                       std::vector<byte>* buffer_out) {
  ASSERT(buffer_out, (L""));

  size_t encoded_len = encoded.size();
  int32 required_len = Base64DecodeGetRequiredLength(encoded_len);

  buffer_out->resize(required_len);

  if (required_len == 0) {
    return S_OK;
  }

  int32 bytes_written = required_len;
  BOOL result = Base64Decode(reinterpret_cast<const char*>(&encoded.front()),
                             encoded_len,
                             &buffer_out->front(),
                             &bytes_written);
  if (!result)
    return E_FAIL;
  ASSERT(bytes_written <= required_len, (L""));
  if (bytes_written < required_len) {
    buffer_out->resize(bytes_written);
  }

  return S_OK;
}

HRESULT Base64::Decode(const CStringA& encoded, std::vector<byte>* buffer_out) {
  ASSERT(buffer_out, (L""));

  size_t encoded_len = encoded.GetLength();
  std::vector<byte> buffer_in(encoded_len);
  if (encoded_len != 0) {
    ::memcpy(&buffer_in.front(), encoded.GetString(), encoded_len);
  }

  return Decode(buffer_in, buffer_out);
}

// Base64 in a CString -> binary
HRESULT Base64::Decode(const CString& encoded, std::vector<byte>* buffer_out) {
  ASSERT(buffer_out, (L""));

  CW2A encoded_a(encoded.GetString());

  size_t encoded_len = ::strlen(encoded_a);
  std::vector<byte> buffer_in(encoded_len);
  if (encoded_len != 0) {
    ::memcpy(&buffer_in.front(), encoded_a, encoded_len);
  }

  return Decode(buffer_in, buffer_out);
}

// Using google SHA-1 algorithms rather than CryptoAPI SHA-1
// algorithms; saves the trouble of dealing with CSPs and the like.

CryptoHash::CryptoHash() {
}

CryptoHash::~CryptoHash() {
}

HRESULT CryptoHash::Compute(const TCHAR* filepath,
                            uint64 max_len,
                            std::vector<byte>* hash_out) {
  ASSERT1(filepath);
  ASSERT1(hash_out);

  std::vector<CString> filepaths;
  filepaths.push_back(filepath);
  return Compute(filepaths, max_len, hash_out);
}

HRESULT CryptoHash::Compute(const std::vector<CString>& filepaths,
                            uint64 max_len,
                            std::vector<byte>* hash_out) {
  ASSERT1(filepaths.size() > 0);
  ASSERT1(hash_out);

  return ComputeOrValidate(filepaths, max_len, NULL, hash_out);
}

HRESULT CryptoHash::Compute(const std::vector<byte>& buffer_in,
                            std::vector<byte>* hash_out) {
  ASSERT1(buffer_in.size() > 0);
  ASSERT1(hash_out);

  return ComputeOrValidate(buffer_in, NULL, hash_out);
}

HRESULT CryptoHash::Validate(const TCHAR* filepath,
                             uint64 max_len,
                             const std::vector<byte>& hash_in) {
  ASSERT1(filepath);
  ASSERT1(hash_in.size() == kHashSize);

  std::vector<CString> filepaths;
  filepaths.push_back(filepath);
  return Validate(filepaths, max_len, hash_in);
}

HRESULT CryptoHash::Validate(const std::vector<CString>& filepaths,
                             uint64 max_len,
                             const std::vector<byte>& hash_in) {
  ASSERT1(hash_in.size() == kHashSize);

  return ComputeOrValidate(filepaths, max_len, &hash_in, NULL);
}


HRESULT CryptoHash::Validate(const std::vector<byte>& buffer_in,
                             const std::vector<byte>& hash_in) {
  ASSERT1(buffer_in.size() > 0);
  ASSERT1(hash_in.size() == kHashSize);

  return ComputeOrValidate(buffer_in, &hash_in, NULL);
}

HRESULT CryptoHash::ComputeOrValidate(const std::vector<CString>& filepaths,
                                      uint64 max_len,
                                      const std::vector<byte>* hash_in,
                                      std::vector<byte>* hash_out) {
  ASSERT1(filepaths.size() > 0);
  ASSERT1(hash_in && !hash_out || !hash_in && hash_out);

  byte buf[1024] = {0};
  SecureHashAlgorithm sha;
  uint64 curr_len = 0;
  for (size_t i = 0; i < filepaths.size(); ++i) {
    scoped_hfile file_handle(::CreateFile(filepaths[i],
                                          FILE_READ_DATA,
                                          FILE_SHARE_READ,
                                          NULL,
                                          OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL,
                                          NULL));
    if (!file_handle) {
      return HRESULTFromLastError();
    }

    if (max_len) {
      LARGE_INTEGER file_size = {0};
      if (!::GetFileSizeEx(get(file_handle), &file_size)) {
        return HRESULTFromLastError();
      }
      curr_len += ((static_cast<uint64>(file_size.HighPart)) << 32) +
                  static_cast<uint64>(file_size.LowPart);
      if (curr_len > max_len) {
        UTIL_LOG(LE, (_T("[CryptoHash::ComputeOrValidate]")
                      _T(" exceed max len][curr_len=%lu][max_len=%lu]"),
                      curr_len, max_len));
        return E_FAIL;
      }
    }

    DWORD bytes_read = 0;
    do {
      if (!::ReadFile(get(file_handle),
                      buf,
                      arraysize(buf),
                      &bytes_read,
                      NULL)) {
        return HRESULTFromLastError();
      }

      if (bytes_read > 0) {
        sha.AddBytes(buf, bytes_read);
      }
    } while (bytes_read == arraysize(buf));
  }
  sha.Finished();

  if (hash_in) {
    int res = ::memcmp(&hash_in->front(), sha.Digest(), kHashSize);
    if (res == 0) {
      return S_OK;
    }

    std::vector<byte> calculated_hash(kHashSize);
    memcpy(&calculated_hash.front(), sha.Digest(), kHashSize);
    CStringA base64_encoded_hash;
    Base64::Encode(calculated_hash, &base64_encoded_hash, false);
    CString hash = AnsiToWideString(base64_encoded_hash,
                                    base64_encoded_hash.GetLength());
    REPORT_LOG(L1, (_T("[actual hash=%s]"), hash));
    return SIGS_E_INVALID_SIGNATURE;
  } else {
    hash_out->resize(kHashSize);
    ::memcpy(&hash_out->front(), sha.Digest(), kHashSize);
    return S_OK;
  }
}

HRESULT CryptoHash::ComputeOrValidate(const std::vector<byte>& buffer_in,
                                      const std::vector<byte>* hash_in,
                                      std::vector<byte>* hash_out) {
  ASSERT1(hash_in && !hash_out || !hash_in && hash_out);

  SecureHashAlgorithm sha;
  if (!buffer_in.empty()) {
    sha.AddBytes(&buffer_in.front(), buffer_in.size());
  }
  sha.Finished();

  if (hash_in) {
    int res = ::memcmp(&hash_in->front(), sha.Digest(), kHashSize);
    return (res == 0) ? S_OK : SIGS_E_INVALID_SIGNATURE;
  } else {
    hash_out->resize(kHashSize);
    ::memcpy(&hash_out->front(), sha.Digest(), kHashSize);
    return S_OK;
  }
}

// To sign data you need a CSP with the proper private key installed.
// To get a signing certificate you start with a PFX file.  This file
// encodes a "certificate store" which can hold more than one
// certificate.  (In general it can hold a certificate chain, but we
// only use the signing certificate.)  There are special APIs to verify
// the format of a PFX file and read it into a new certificate store.  A
// password must be specified to read the PFX file as it is encrypted.
// The password was set when the PFX file was exported or otherwise
// created.  Then you search for the proper certificate in the store
// (using the subject_name which tells who the certificate was issued
// to).  Finally, to get a CSP with the certificate's private key
// available there is a special API, CryptAcquireCertificatePrivateKey,
// that takes a CSP and a certificate and makes the private key of the
// certificate the private key of the CSP.

CryptoSigningCertificate::CryptoSigningCertificate() : key_spec_(0) {
}

CryptoSigningCertificate::~CryptoSigningCertificate() {
}

HRESULT CryptoSigningCertificate::ImportCertificate(
    const TCHAR * filepath,
    const TCHAR * password,
    const TCHAR * subject_name) {
  ASSERT(filepath, (L""));
  ASSERT(password, (L""));

  std::vector<byte> buffer;
  HRESULT hr = ReadEntireFile(filepath, kMaxCertificateSize, &buffer);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (L"[CryptoSigningCertificate::ImportCertificate]"
                  L"['%s' not read, hr 0x%08lx]", filepath, hr));
    return hr;
  }
  return ImportCertificate(buffer, password, subject_name);
}

HRESULT CryptoSigningCertificate::ImportCertificate(
    const std::vector<byte>& certificate_in,
    const TCHAR * password,
    const TCHAR * subject_name) {
  ASSERT(password, (L""));
  ASSERT1(!certificate_in.empty());

  UTIL_LOG(L2, (L"[CryptoSigningCertificate::ImportCertificate]"
                L"[%d bytes, subject_name '%s']",
                certificate_in.size(), subject_name ? subject_name : L""));

  // CryptoAPI treats the certificate as a "blob"
  CRYPT_DATA_BLOB blob;
  blob.cbData = certificate_in.size();
  blob.pbData = const_cast<BYTE*>(&certificate_in.front());

  // Ensure that it is PFX formatted
  BOOL b = ::PFXIsPFXBlob(&blob);
  if (!b) {
    ASSERT(0, (L"Invalid PFX certificate, err 0x%08lx", ::GetLastError()));
    return SIGS_E_INVALID_PFX_CERTIFICATE;
  }

  // Make sure the password checks out
  b = ::PFXVerifyPassword(&blob, password, 0 /* flags */);
  if (!b) {
    UTIL_LOG(LE, (L"[CryptoSigningCertificate::ImportCertificate]"
                  L"[invalid password, err 0x%08lx]", ::GetLastError()));
    return SIGS_E_INVALID_PASSWORD;
  }

  // Do the import from the certificate to a new certificate store
  // TODO(omaha): Check that this is in fact a new certificate store, not an
  // existing one.  If it is an existing one we'll need to delete the
  // certificate later.
  // The last parameter to ::PFXImportCertStore() is 0, indicating that we want
  // the CSP to be "silent"; i.e., not prompt.
  reset(store_, ::PFXImportCertStore(&blob, password, 0));
  if (!store_) {
    DWORD err = ::GetLastError();
    ASSERT(0, (L"Failed to import PFX certificate into a certificate store, "
               L"err 0x%08lx", err));
    return HRESULT_FROM_WIN32(err);
  }
  UTIL_LOG(L3, (L"[CryptoSigningCertificate::ImportCertificate]"
                L"[new store 0x%08lx]", get(store_)));

  // Now that we have a store, look for the correct certificate.  (There may
  // have been more than one in the PFX file, e.g., a certificate chain.)
  PCCERT_CONTEXT certificate_context = NULL;
  while ((certificate_context =
          ::CertEnumCertificatesInStore(get(store_),
                                        certificate_context)) != NULL) {
    // Have a certificate, does it look like the right one?  Check the name
    DWORD name_len = ::CertGetNameString(certificate_context,
                                         kCertificateNameType,
                                         0 /*flags*/,
                                         NULL,
                                         NULL,
                                         0);
    if (name_len <= 1) {
      // Name attribute not found - should never happen
      ASSERT(0, (L"CryptoSigningCertificate::ImportCertificate failed to get "
                 L"certificate name length, err 0x%08lx", ::GetLastError()));
      continue;
    }
    // name_len includes the terminating null

    std::vector<TCHAR> name;
    name.resize(name_len);
    ASSERT1(!name.empty());
    DWORD name_len2 = ::CertGetNameString(certificate_context,
                                          kCertificateNameType,
                                          0,
                                          NULL,
                                          &name.front(),
                                          name_len);
    ASSERT(name_len2 == name_len, (L""));

    UTIL_LOG(L3, (L"[CryptoSigningCertificate::ImportCertificate]"
                  L"[found '%s' in store]", &name.front()));

    // Check the name if the user so desires.  (If subject_name == NULL then
    // the first certificate found is used.)
    if (subject_name && (0 != String_StrNCmp(&name.front(),
                                             subject_name,
                                             ::lstrlen(subject_name),
                                             false))) {
      // name mismatch
      UTIL_LOG(L3, (L"[CryptoSigningCertificate::ImportCertificate]"
                    L"[not the right certificate, we're looking for '%s']",
                    subject_name));
      continue;
    }

    // This is the right certificate
    subject_name_ = &name.front();
    reset(certificate_, certificate_context);
    UTIL_LOG(L3, (L"[CryptoSigningCertificate::ImportCertificate]"
                  L"[new certificate 0x%08lx]", get(certificate_)));
    break;
  }

  return S_OK;
}

HRESULT CryptoSigningCertificate::GetCSPContext(HCRYPTPROV* csp_context) {
  ASSERT(csp_context, (L""));
  ASSERT(get(certificate_), (L""));

  // CSP may have already been used - reset it
  reset(csp_);

  // Create a CSP context using the private key of the certificate we imported
  // earlier.
  HCRYPTPROV csp = NULL;
  BOOL must_free_csp = FALSE;
  BOOL b = ::CryptAcquireCertificatePrivateKey(get(certificate_),
                                               0 /*flags*/,
                                               0 /*reserved*/,
                                               &csp,
                                               &key_spec_,
                                               &must_free_csp);
  if (!b) {
    DWORD err = ::GetLastError();
    ASSERT(0, (L"CryptoSigningCertificate::GetCSPContext "
               L"CryptAcquireCertificatePrivateKey failed, err 0x%08lx", err));
    return HRESULT_FROM_WIN32(err);
  }

  // (Funky API returns a boolean which tells you whether it is your
  // responsibility to delete the CSP context or not.)
  if (must_free_csp) {
    reset(csp_, csp);
  }
  if (get(csp_)) {
    UTIL_LOG(L3, (L"[CryptoSigningCertificate::GetCSPContext new CSP 0x%08lx]",
                  get(csp_)));
  }

  ASSERT(key_spec_ == AT_SIGNATURE || key_spec_ == AT_KEYEXCHANGE, (L""));
  if (key_spec_ != kKeyPairType) {
    UTIL_LOG(LE, (L"[CryptoSigningCertificate::GetCSPContext]"
                  L"[requires a AT_SIGNATURE type key]"));
    return SIGS_E_INVALID_KEY_TYPE;
  }

#ifdef _DEBUG
  // Which CSP did we get?
  char csp_name[256] = {0};
  DWORD csp_name_len = arraysize(csp_name);
  b = ::CryptGetProvParam(csp,
                          PP_NAME,
                          reinterpret_cast<BYTE*>(&csp_name[0]),
                          &csp_name_len,
                          0 /*flags*/);
  if (!b) {
    DWORD err = ::GetLastError();
    UTIL_LOG(LE, (L"[CryptoSigningCertificate::GetCSPContext]"
                  L"[error getting CSP name, err 0x%08lx]", err));
  }
  DWORD csp_prov_type;
  DWORD csp_prov_type_len = sizeof(csp_prov_type);
  b = ::CryptGetProvParam(csp,
                          PP_PROVTYPE,
                          reinterpret_cast<BYTE*>(&csp_prov_type),
                          &csp_prov_type_len,
                          0 /*flags*/);
  if (!b) {
    DWORD err = ::GetLastError();
    UTIL_LOG(LE, (L"[CryptoSigningCertificate::GetCSPContext]"
                  L"[error getting CSP provtype, err 0x%08lx]", err));
  }
  char csp_container[256] = {0};
  DWORD csp_container_len = arraysize(csp_container);
  b = ::CryptGetProvParam(csp,
                          PP_CONTAINER,
                          reinterpret_cast<BYTE*>(&csp_container[0]),
                          &csp_container_len,
                          0 /*flags*/);
  if (!b) {
    DWORD err = ::GetLastError();
    UTIL_LOG(LE, (L"[CryptoSigningCertificate::GetCSPContext]"
                  L"[error getting CSP current container name, err 0x%08lx]",
                  err));
  }
  UTIL_LOG(L2, (L"[CryptoSigningCertificate::GetCSPContext]"
                L"[have CSP '%S' (provtype %d) key container '%S']",
                csp_name, csp_prov_type, csp_container));
  // End of which CSP did we get
#endif

  *csp_context = csp;

  UTIL_LOG(L2, (L"[CryptoSigningCertificate::GetCSPContext]"
                L"[getting CSP with private key from certificate]"
                L"[HCRYPTPROV 0x%08lx]", csp));

  return S_OK;
}

// To sign some data using CryptoAPI you first hash it into a hash
// object, then sign it using the CSP.  The CSP needs to have the
// private key, of type AT_SIGNATURE, in it already, as it isn't a
// parameter of the CryptSignHash API.  The CryptoSigningCertificate
// can provide such a CSP.

CryptoComputeSignature::CryptoComputeSignature(
    CryptoSigningCertificate* certificate)
    : certificate_(certificate) {
}

CryptoComputeSignature::~CryptoComputeSignature() {
}

HRESULT CryptoComputeSignature::Sign(TCHAR const * const filepath,
                                     uint32 max_len,
                                     std::vector<byte>* signature_out) {
  ASSERT(filepath, (L""));
  std::vector<byte> buffer;
  HRESULT hr = ReadEntireFile(filepath, max_len, &buffer);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (L"[CryptoComputeSignature::Sign]"
                  L"['%s not read, hr 0x%08lx]", filepath, hr));
    return hr;
  }
  return Sign(buffer, signature_out);
}

HRESULT CryptoComputeSignature::Sign(const std::vector<byte>& buffer_in,
                                     std::vector<byte>* signature_out) {
  ASSERT(signature_out, (L""));
  ASSERT1(!buffer_in.empty());

  UTIL_LOG(L2, (L"[CryptoComputeSignature::Sign]"
                L"[buffer of %d bytes]", buffer_in.size()));

  // Get the proper CSP with the private key (certificate retains ownership)
  HCRYPTPROV csp = NULL;
  HRESULT hr = certificate_->GetCSPContext(&csp);
  ASSERT(SUCCEEDED(hr) && csp, (L""));

  // Hash the data
  CryptDetails::scoped_crypt_hash hash;
  BOOL b = ::CryptCreateHash(csp, kHashAlgorithm, 0, 0, address(hash));
  if (!b) {
    // hash is now invalid, but might not be NULL, so stomp on it
    DWORD err = ::GetLastError();
    ASSERT(!hash, (L""));
    UTIL_LOG(LE, (L"[CryptoComputeSignature::Sign]"
                  L"[could not create hash, err 0x%08lx]", err));
    return HRESULT_FROM_WIN32(err);
  }
  UTIL_LOG(L3, (L"CryptoComputeSignature::Sign new hash 0x%08lx", get(hash)));

  b = ::CryptHashData(get(hash), &buffer_in.front(), buffer_in.size(), 0);
  if (!b) {
    DWORD err = ::GetLastError();
    UTIL_LOG(LE, (L"[CryptoComputeSignature::Sign]"
                  L"[could not hash data, err 0x%08lx]", err));
    return HRESULT_FROM_WIN32(err);
  }

  // Sign the hash (first get length, then allocate buffer and do real signing)
  DWORD signature_len = 0;
  b = ::CryptSignHash(get(hash),
                      kKeyPairType,
                      NULL,
                      0 /*flags*/,
                      NULL,
                      &signature_len);
  if (!b && ::GetLastError() != ERROR_MORE_DATA) {
    DWORD err = ::GetLastError();
    UTIL_LOG(LE, (L"[CryptoComputeSignature::Sign]"
                  L"[could not compute size of signature, err 0x%08lx]", err));
    return HRESULT_FROM_WIN32(err);
  }
  signature_out->resize(signature_len);
  b = ::CryptSignHash(get(hash),
                      kKeyPairType,
                      NULL,
                      0,
                      &signature_out->front(),
                      &signature_len);
  if (!b) {
    DWORD err = ::GetLastError();
    UTIL_LOG(LE, (L"[CryptoComputeSignature::Sign]"
                  L"[could not compute signature, err 0x%08lx]", err));
    return HRESULT_FROM_WIN32(err);
  }
  ASSERT(signature_len == signature_out->size(), (L""));

  UTIL_LOG(L3, (L"[CryptoComputeSignature::Sign]"
                L"[have %d byte signature]", signature_out->size()));

  return S_OK;
}

// To verify signed data you need a CSP, and you also need the public
// key extracted from a certificate.  The CSP can be any RSA CSP on the
// machine, the default one is fine.  To get the public key you start
// by importing a certificate in standard "DER encoded" format.  That
// returns a giant data structure, one field of which is the public key
// in a format that CryptoAPI understands.  You import this public key
// into the CSP with the CryptImportPublicKey() API, and then create a
// key object from it suitable for use with the verification API.

CryptoSignatureVerificationCertificate::CryptoSignatureVerificationCertificate() {   // NOLINT
}

CryptoSignatureVerificationCertificate::~CryptoSignatureVerificationCertificate() {  // NOLINT
}

HRESULT CryptoSignatureVerificationCertificate::ImportCertificate(
    const TCHAR * filepath,
    const TCHAR * subject_name) {
  ASSERT(filepath, (L""));
  std::vector<byte> buffer;
  HRESULT hr = ReadEntireFile(filepath, kMaxCertificateSize, &buffer);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (L"[CryptoSignatureVerificationCertificate::ImportCertificate]"
                  L"['%s' not read, hr 0x%08lx]", filepath, hr));
    return hr;
  }
  return ImportCertificate(buffer, subject_name);
}

HRESULT CryptoSignatureVerificationCertificate::ImportCertificate(
    const std::vector<byte>& certificate_in,
    const TCHAR * subject_name) {
  // Import the certificate
  ASSERT1(!certificate_in.empty());
  reset(certificate_, ::CertCreateCertificateContext(kEncodingType,
                                                     &certificate_in.front(),
                                                     certificate_in.size()));
  if (!certificate_) {
    DWORD err = ::GetLastError();
    UTIL_LOG(LE, (L"[CryptoSignatureVerificationCertificate::ImportCertificate]"
                  L"[could not import certificate, err 0x%08lx]", err));
    return SIGS_E_INVALID_DER_CERTIFICATE;
  }
  UTIL_LOG(L3, (L"[CryptoSignatureVerificationCertificate::ImportCertificate]"
                L"[new certificate 0x%08lx]", get(certificate_)));

  // Get certificate's subject name
  DWORD name_len = ::CertGetNameString(get(certificate_),
                                       kCertificateNameType,
                                       0 /*flags*/,
                                       NULL,
                                       NULL,
                                       0);
  if (name_len <= 1) {
    // Name attribute not found - should never happen
    ASSERT(0, (L"CryptoSignatureVerificationCertificate failed to get "
               L"certificate name length, err 0x%08lx", ::GetLastError()));
    return E_FAIL;
  }
  // name_len includes the terminating NULL

  std::vector <TCHAR> name;
  name.resize(name_len);
  ASSERT1(!name.empty());
  DWORD name_len2 = ::CertGetNameString(get(certificate_),
                                        kCertificateNameType,
                                        0,
                                        NULL,
                                        &name.front(),
                                        name_len);
  ASSERT(name_len2 == name_len, (L""));

  UTIL_LOG(L3, (L"[CryptoSignatureVerificationCertificate::ImportCertificate]"
                L"['%s' is subject of certificate]", &name.front()));

  subject_name_ = &name.front();

  // Check the name if the user so desires.
  if (subject_name && (0 != String_StrNCmp(&name.front(),
                                           subject_name,
                                           ::lstrlen(subject_name), false))) {
      // name mismatch
    UTIL_LOG(L3, (L"[CryptoSignatureVerificationCertificate::ImportCertificate]"
                  L"[not the right certificate, we're looking for '%s']",
                  subject_name));
    return E_FAIL;
  }

  return S_OK;
}

HRESULT CryptoSignatureVerificationCertificate::GetCSPContextAndKey(
    HCRYPTPROV* csp_context,
    HCRYPTKEY* public_key) {
  ASSERT(csp_context, (L""));
  ASSERT(public_key, (L""));
  ASSERT(get(certificate_), (L""));

  // Get the public key out of the certificate
  PCERT_INFO cert_info = get(certificate_)->pCertInfo;
  ASSERT(cert_info, (L""));
  PCERT_PUBLIC_KEY_INFO public_key_info = &cert_info->SubjectPublicKeyInfo;
  ASSERT(public_key_info, (L""));

  // Reset the CSP and key in case it has been used already
  reset(key_);
  reset(csp_);

  // Get the default CSP.  With CRYPT_VERIFYCONTEXT don't need to worry
  // about creating/destroying a key container.
  // TODO(omaha):  Why wasn't PROV_RSA_SIG available?  Maybe looking for the
  // default isn't a good idea?
  BOOL b = ::CryptAcquireContext(address(csp_),
                                 NULL,
                                 NULL,
                                 kProviderType,
                                 CRYPT_VERIFYCONTEXT|CRYPT_SILENT);
  if (!b) {
    DWORD err = ::GetLastError();
    UTIL_LOG(LE, (L"[GetCSPContextAndKey]"
                  L"[failed to acquire CSP, err 0x%08lx]", err));
    return HRESULT_FROM_WIN32(err);
  }
  UTIL_LOG(L3, (L"[CryptoSignatureVerificationCertificate::GetCSPContextAndKey]"
                L"[new CSP 0x%08lx]", get(csp_)));

  // Convert the public key in encoded form into a CryptoAPI HCRYPTKEY
  b = ::CryptImportPublicKeyInfo(get(csp_),
                                 kEncodingType,
                                 public_key_info,
                                 address(key_));
  if (!b) {
    DWORD err = ::GetLastError();
    UTIL_LOG(LE, (L"[GetCSPContextAndKey]"
                  L"[failed to import public key, err 0x%08lx]", err));
    return HRESULT_FROM_WIN32(err);
  }
  UTIL_LOG(L3, (L"[CryptoSignatureVerificationCertificate::GetCSPContextAndKey]"
                L"[new key 0x%08lx]", get(key_)));

  *csp_context = get(csp_);
  *public_key = get(key_);

  return S_OK;
}

// To verify the signature of some data using CryptoAPI you first hash
// it into a hash object, then verify it using the CSP and a public key.
// In this case the CryptVerifySignature takes the key (of type
// AT_SIGNATURE) as a separate parameter. The
// CryptoSignatureVerificationCertificate can provide the proper CSP and
// the public key from the certificate.

CryptoVerifySignature::CryptoVerifySignature(
    CryptoSignatureVerificationCertificate& certificate)
    : certificate_(&certificate) {
}

CryptoVerifySignature::~CryptoVerifySignature() {
}

HRESULT CryptoVerifySignature::Validate(const TCHAR* filepath,
                                        uint32 max_len,
                                        const std::vector<byte>& signature_in) {
  ASSERT(filepath, (L""));
  std::vector<byte> buffer;
  HRESULT hr = ReadEntireFile(filepath, max_len, &buffer);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (L"[CryptoVerifySignature::Validate]"
                  L"['%s' not read, hr 0x%08lx]", filepath, hr));
    return hr;
  }
  return Validate(buffer, signature_in);
}

HRESULT CryptoVerifySignature::Validate(const std::vector<byte>& buffer_in,
                                        const std::vector<byte>& signature_in) {
  ASSERT(certificate_, (L""));
  ASSERT1(!buffer_in.empty());
  ASSERT1(!signature_in.empty());

  UTIL_LOG(L2, (L"[CryptoVerifySignature::Validate]"
                L"[buffer of %d bytes, signature of %d bytes]",
                buffer_in.size(), signature_in.size()));

  // Get the CSP context and the public key from the certificate
  HCRYPTPROV csp = NULL;
  HCRYPTKEY key = NULL;
  HRESULT hr = certificate_->GetCSPContextAndKey(&csp, &key);
  ASSERT(SUCCEEDED(hr) && csp && key, (L""));

  // Hash the data
  CryptDetails::scoped_crypt_hash hash;
  BOOL b = ::CryptCreateHash(csp, kHashAlgorithm, 0, 0, address(hash));
  if (!b) {
    // hash is now invalid, but might not be NULL, so stomp on it
    DWORD err = ::GetLastError();
    ASSERT(!hash, (L""));
    UTIL_LOG(LE, (L"[CrypoVerifySignature::Validate]"
                  L"[could not create hash], err 0x%08lx", err));
    return HRESULT_FROM_WIN32(err);
  }
  UTIL_LOG(L3, (L"CryptoVerifySignature::Validate new hash 0x%08lx", hash));

  b = ::CryptHashData(get(hash),
                      &buffer_in.front(),
                      buffer_in.size(),
                      0 /*flags*/);
  if (!b) {
    DWORD err = ::GetLastError();
    UTIL_LOG(LE, (L"[CryptoVerifySignature::Validate]"
                  L"[could not hash data, err 0x%08lx]", err));
    return HRESULT_FROM_WIN32(err);
  }

  // Verify the hash
  b = ::CryptVerifySignature(get(hash),
                             &signature_in.front(),
                             signature_in.size(),
                             key,
                             NULL,
                             0 /*flags*/);
  if (!b) {
    DWORD err = ::GetLastError();
#ifdef LOGGING
    CString encoded_signature;
    Base64::Encode(signature_in, &encoded_signature, false);

    UTIL_LOG(LE, (_T("CryptoVerifySignature::Validate could not ")
                  _T("verify signature, err 0x%08lx with sig \"%s\""),
                  err, encoded_signature));
#endif
    if (err == NTE_BAD_SIGNATURE)
      return SIGS_E_INVALID_SIGNATURE;
    else
      return HRESULT_FROM_WIN32(err);
  }

  return S_OK;
}

HRESULT SignData(const TCHAR* certificate_path,
                 const TCHAR* certificate_password,
                 const TCHAR* certificate_subject_name,
                 const std::vector<byte>& data,
                 CString* signature_base64) {
  ASSERT(certificate_path, (L""));
  ASSERT(certificate_password, (L""));
  // certificate_subject_name can be NULL
  ASSERT(signature_base64, (L""));

  CryptoSigningCertificate certificate;
  RET_IF_FAILED(certificate.ImportCertificate(certificate_path,
                                              certificate_password,
                                              certificate_subject_name));

  CryptoComputeSignature signer(&certificate);
  std::vector<byte> signature;
  RET_IF_FAILED(signer.Sign(data, &signature));
  RET_IF_FAILED(Base64::Encode(signature, signature_base64, false));

  return S_OK;
}

HRESULT VerifyData(const TCHAR* certificate_path,
                   const TCHAR* certificate_subject_name,
                   const std::vector<byte>& data,
                   const TCHAR* signature_base64) {
  ASSERT(certificate_path, (L""));
  // certificate_subject_name can be NULL
  ASSERT(signature_base64, (L""));

  std::vector<byte> signature;
  RET_IF_FAILED(Base64::Decode(CString(signature_base64), &signature));

  CryptoSignatureVerificationCertificate certificate;
  RET_IF_FAILED(certificate.ImportCertificate(certificate_path,
                                              certificate_subject_name));

  CryptoVerifySignature verifier(certificate);
  RET_IF_FAILED(verifier.Validate(data, signature));

  return S_OK;
}

HRESULT VerifyData(const std::vector<byte>& certificate_buffer,
                   const TCHAR* certificate_subject_name,
                   const std::vector<byte>& data,
                   const TCHAR* signature_base64) {
  // certificate_subject_name can be NULL
  ASSERT(signature_base64, (L""));

  std::vector<byte> signature;
  RET_IF_FAILED(Base64::Decode(CString(signature_base64), &signature));

  CryptoSignatureVerificationCertificate certificate;
  RET_IF_FAILED(certificate.ImportCertificate(certificate_buffer,
                                              certificate_subject_name));

  CryptoVerifySignature verifier(certificate);
  RET_IF_FAILED(verifier.Validate(data, signature));

  return S_OK;
}

// Authenticate files
HRESULT AuthenticateFiles(const std::vector<CString>& files,
                          const CString& hash) {
  ASSERT1(files.size() > 0);
  ASSERT1(!hash.IsEmpty());

  // Test the bytes against its hash
  std::vector<byte> hash_vector;
  RET_IF_FAILED(Base64::Decode(hash, &hash_vector));
  ASSERT1(hash_vector.size() == CryptoHash::kHashSize);

  CryptoHash crypto;
  return crypto.Validate(files, kMaxFileSizeForAuthentication, hash_vector);
}

}  // namespace omaha

