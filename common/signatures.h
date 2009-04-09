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
// signatures.h
//
// Classes and functions related to crypto-hashes of buffers and digital
// signatures of buffers.

#ifndef OMAHA_COMMON_SIGNATURES_H__
#define OMAHA_COMMON_SIGNATURES_H__

#include <windows.h>
#include <wincrypt.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/sha.h"

namespace omaha {

// Forward decls of classes defined here
class CryptoHash;
class CryptoComputeSignature;
class CryptoVerifySignature;
class CryptoSigningCertificate;
class CryptoSignatureVerificationCertificate;

// Useful scoped pointers for working with CryptoAPI objects
namespace CryptDetails {

  void crypt_close_store(HCERTSTORE);
  void crypt_release_context(HCRYPTPROV);
  void crypt_free_certificate(PCCERT_CONTEXT);
  void crypt_destroy_key(HCRYPTKEY);

  typedef close_fun<void (*)(HCERTSTORE),crypt_close_store>          smart_close_store;       // NOLINT
  typedef close_fun<void (*)(HCRYPTPROV),crypt_release_context>      smart_release_context;   // NOLINT
  typedef close_fun<void (*)(PCCERT_CONTEXT),crypt_free_certificate> smart_free_certificate;  // NOLINT
  typedef close_fun<void (*)(HCRYPTKEY),crypt_destroy_key>           smart_destroy_key;       // NOLINT

  typedef scoped_any<HCERTSTORE,smart_close_store,null_t>            scoped_crypt_store;      // NOLINT
  typedef scoped_any<HCRYPTPROV,smart_release_context,null_t>        scoped_crypt_context;    // NOLINT
  typedef scoped_any<PCCERT_CONTEXT,smart_free_certificate,null_t>   scoped_crypt_cert;       // NOLINT
  typedef scoped_any<HCRYPTKEY,smart_destroy_key,null_t>             scoped_crypt_key;        // NOLINT
}

// A namespace for encoding binary into base64 (portable ASCII) representation
// and for decoding it again.
namespace Base64 {
  // Binary -> base64 in a buffer
  HRESULT Encode(const std::vector<byte>& buffer_in,
                 std::vector<byte>* encoded,
                 bool break_into_lines = true);

  // Binary -> base64 in a string
  HRESULT Encode(const std::vector<byte>& buffer_in,
                 CStringA* encoded,
                 bool break_into_lines = true);

  // Binary -> base64 in a wide string
  HRESULT Encode(const std::vector<byte>& buffer_in,
                 CString* encoded,
                 bool break_into_lines = true);

  // Base64 in a buffer -> binary
  HRESULT Decode(const std::vector<byte>& encoded,
                 std::vector<byte>* buffer_out);

  // Base64 in a CStringA -> binary
  HRESULT Decode(const CStringA& encoded, std::vector<byte>* buffer_out);

  // Base64 in a CString -> binary
  HRESULT Decode(const CString& encoded, std::vector<byte>* buffer_out);
}


// Compute and validate SHA-1 hashes of data
class CryptoHash {
  public:

    CryptoHash();
    ~CryptoHash();

    static const int kHashSize = SecureHashAlgorithm::kDigestSize;

    // Hash a file
    HRESULT Compute(const TCHAR * filepath,
                    uint64 max_len,
                    std::vector<byte>* hash_out);

    // Hash a list of files
    HRESULT Compute(const std::vector<CString>& filepaths,
                    uint64 max_len,
                    std::vector<byte>* hash_out);

    // Hash a buffer
    HRESULT Compute(const std::vector<byte>& buffer_in,
                    std::vector<byte>* hash_out);

    // Verify hash of a file
    HRESULT Validate(const TCHAR * filepath,
                     uint64 max_len,
                     const std::vector<byte>& hash_in);

    // Verify hash of a list of files
    HRESULT Validate(const std::vector<CString>& filepaths,
                     uint64 max_len,
                     const std::vector<byte>& hash_in);

    // Verify hash of a buffer
    HRESULT Validate(const std::vector<byte>& buffer_in,
                     const std::vector<byte>& hash_in);

  private:
    // Compute or verify hash of a file
    HRESULT ComputeOrValidate(const std::vector<CString>& filepaths,
                              uint64 max_len,
                              const std::vector<byte>* hash_in,
                              std::vector<byte>* hash_out);

    // Compute or verify hash of a buffer
    HRESULT ComputeOrValidate(const std::vector<byte>& buffer_in,
                              const std::vector<byte>* hash_in,
                              std::vector<byte>* hash_out);

    DISALLOW_EVIL_CONSTRUCTORS(CryptoHash);
};


// Import and use a certificate for signing data (has a private key)
class CryptoSigningCertificate {
  public:
    CryptoSigningCertificate();
    ~CryptoSigningCertificate();

    // Import certificate - with both public key and private key.
    // Must be in PFX format.  Password must unlock the PFX file.
    // subject_name is the certificate's subject name (who it was
    // issued to) - if not NULL then it is checked for an exact
    // match against the certificate.

    // User can get the certificate in PFX format by following the procedure at
    // http://support.globalsign.net/en/objectsign/transform.cfm

    HRESULT ImportCertificate(const TCHAR * filepath,
                              const TCHAR * password,
                              const TCHAR * subject_name);

    HRESULT ImportCertificate(const std::vector<byte>& certificate_in,
                              const TCHAR * password,
                              const TCHAR * subject_name);

    CString subject_name() { return subject_name_; }

  private:
    static const int kMaxCertificateSize = 100000;

    CryptDetails::scoped_crypt_store   store_;
    CryptDetails::scoped_crypt_cert    certificate_;
    CryptDetails::scoped_crypt_context csp_;
    CString subject_name_;
    DWORD key_spec_;

    friend class CryptoComputeSignature;
    // Get the CSP with the private key
    // (CryptoSigningCertificate retains ownership of csp_context.)
    HRESULT GetCSPContext(HCRYPTPROV* csp_context);

    DISALLOW_EVIL_CONSTRUCTORS(CryptoSigningCertificate);
};


// Compute digital signatures
class CryptoComputeSignature {
  public:
    explicit CryptoComputeSignature(CryptoSigningCertificate* certificate);
    ~CryptoComputeSignature();

    // Sign a file, returning a separate signature
    HRESULT Sign(const TCHAR * filepath,
                 uint32 max_len,
                 std::vector<byte>* signature_out);

    // Sign a chunk of memory, returning a separate signature
    HRESULT Sign(const std::vector<byte>& buffer_in,
                 std::vector<byte>* signature_out);

  private:
    // Does not take ownership of the certificate
    CryptoSigningCertificate* const certificate_;

    DISALLOW_EVIL_CONSTRUCTORS(CryptoComputeSignature);
};


// Import and use a certificate for verifying signatures (has public key only)
class CryptoSignatureVerificationCertificate {
  public:
    CryptoSignatureVerificationCertificate();
    ~CryptoSignatureVerificationCertificate();

    // Import certificate - with only public key.  Must be in DER (.cer) format.
    // subject_name is the certificate's subject name (who it was
    // issued to) - if not NULL then it is checked for an exact
    // match against the certificate.

    // User can get certificate in DER format (.cer) by exporting from
    // certmgr.exe, using openssl, etc.)

    HRESULT ImportCertificate(const TCHAR * filepath,
                              const TCHAR * subject_name);
    HRESULT ImportCertificate(const std::vector<byte>& certificate_in,
                              const TCHAR * subject_name);

    CString subject_name() { return subject_name_; }

  private:
    static const int kMaxCertificateSize = 100000;

    CryptDetails::scoped_crypt_cert    certificate_;
    CryptDetails::scoped_crypt_context csp_;
    CryptDetails::scoped_crypt_key     key_;
    CString subject_name_;

    friend class CryptoVerifySignature;
    // Get the CSP and the public key
    // (CryptoSignatureVerificationCertificate retains ownership of csp_context
    // and public_key.)
    HRESULT GetCSPContextAndKey(HCRYPTPROV* csp_context, HCRYPTKEY* public_key);

    DISALLOW_EVIL_CONSTRUCTORS(CryptoSignatureVerificationCertificate);
};


// Verify digital signatures
class CryptoVerifySignature {
  public:
    explicit CryptoVerifySignature(
        CryptoSignatureVerificationCertificate& certificate);
    ~CryptoVerifySignature();

    // Validate signature of a file, signature given separately
    HRESULT Validate(const TCHAR * filepath,
                     uint32 max_len,
                     const std::vector<byte>& signature_in);

    // Validate signature of a buffer of data, signature given separately
    HRESULT Validate(const std::vector<byte>& buffer_in,
                     const std::vector<byte>& signature_in);

  private:
    // Does not take ownership of the certificate
    CryptoSignatureVerificationCertificate* const certificate_;

    DISALLOW_EVIL_CONSTRUCTORS(CryptoVerifySignature);
};


// All-in-one routine to sign a chunk of data and return the signature
// (encoded in base64)
HRESULT SignData(const TCHAR* certificate_path,
                 const TCHAR* certificate_password,
                 const TCHAR* certificate_subject_name,
                 const std::vector<byte>& data,
                 CString* signature_base64);

// All-in-one routine to verify the signature of a chunk of data
HRESULT VerifyData(const TCHAR* certificate_path,
                   const TCHAR* certificate_subject_name,
                   const std::vector<byte>& data,
                   const TCHAR* signature_base64);

HRESULT VerifyData(const std::vector<byte>& certificate_buffer,
                   const TCHAR* certificate_subject_name,
                   const std::vector<byte>& data,
                   const TCHAR* signature_base64);

// Authenticate files
HRESULT AuthenticateFiles(const std::vector<CString>& files,
                          const CString& hash);

}  // namespace omaha

#endif  // OMAHA_COMMON_SIGNATURES_H__
